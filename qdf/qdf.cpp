#include "qdf.h"
#include "quickwritelist.h"
#include <stdio.h>
using namespace ng::qdf;


#define QDF_COMMENT                 "//"
#define QDF_MULTILINE_COMMENT_BEGIN "/*"
#define QDF_MULTILINE_COMMENT_END   "*/"
#define QDF_LIST_BEGIN              '('
#define QDF_LIST_END                ')'
#define QDF_SUBBLOCK_BEGIN          '{'
#define QDF_SUBBLOCK_END            '}'
#define QDF_STRING_CONTAINER        '\"'





/////////////////
// QDF Helpers //
/////////////////


// Will return true on end of string!
inline bool isWhitespace(char c)
{
	return (c < '!' || c > '~');
}

// Terrible
inline bool isControlCharacterExcludeComment(char c)
{
	return c == QDF_LIST_BEGIN
		|| c == QDF_LIST_END
		|| c == QDF_SUBBLOCK_BEGIN
		|| c == QDF_SUBBLOCK_END
		|| c == QDF_STRING_CONTAINER;
}

// Terrible
inline bool isControlCharacter(const char* c)
{
	char v = *c;
	return isControlCharacterExcludeComment(v)
		|| strncmp(c, QDF_COMMENT, sizeof(QDF_COMMENT) - 1) == 0
		|| strncmp(c, QDF_MULTILINE_COMMENT_BEGIN, sizeof(QDF_MULTILINE_COMMENT_BEGIN) - 1) == 0;
		// End of multiline is only considered for the actual comment
}


struct QDFToken
{
	const char* str;
	size_t len;
	QDFToken* parent = nullptr;
	QuickWriteList<QDFToken>::Location skip = { 0,0 };
};

inline bool isChar(QDFToken* tok, char c)
{
	return tok->len == 1 && *tok->str == c;
}


// Could be merged into QDFParser directly, but oh well
// Wrapper over qdf's input with helper utils 
class QDFInput
{
public:
	QDFInput() : QDFInput(nullptr, 0) {}

	QDFInput(const char* in, size_t len)
	{
		error = QDFParseError::NONE;
		cur = input = in;
		length = len;
	}

	// We pass in max size_t since it'll always be higher than our position
	QDFInput(const char* in) : QDFInput(in, SIZE_MAX) {}

	// Checks if the input is currently valid
	inline bool valid() { return *cur && (cur - input) < length; }

	// Skips over all whitespace and comments and returns the current character in the string
	char skip()
	{
		do
		{
			// Skip over all whitespace
			for (; valid() && isWhitespace(*cur); cur++);

			// 1 is subtracted off sizeof due to the string having a 0 at the end
			// Offset by length of comment so we begin within the comment

			// Check for a single line comment
			if (strncmp(cur, QDF_COMMENT, sizeof(QDF_COMMENT) - 1) == 0)
			{
				// Skip until end line
				// We can just use \n here since it's the last char for an endline on both nix and windows
				for (cur += sizeof(QDF_COMMENT) - 1; valid() && *cur != '\n'; cur++);
			}
			// Check for a multiline comment
			else if (strncmp(cur, QDF_MULTILINE_COMMENT_BEGIN, sizeof(QDF_MULTILINE_COMMENT_BEGIN) - 1) == 0)
			{
				// Skip until we hit an end of a mutliline comment 
				for (cur += sizeof(QDF_MULTILINE_COMMENT_BEGIN) - 1; valid() && strncmp(cur, QDF_MULTILINE_COMMENT_END, sizeof(QDF_MULTILINE_COMMENT_END) - 1); cur++);

				// If we're invalid at the end of a comment, we failed to reach our end of multiline
				if (!valid())
				{
					error = QDFParseError::UNCLOSED_COMMENT;
					return 0;
				}

				// Move over the end of comment
				cur += sizeof(QDF_MULTILINE_COMMENT_END) - 1;
			}

			// Loop back to the start if we have more whitespace
		} while (isWhitespace(*cur) && valid());
		
		return *cur;
	}

	QDFToken readString()
	{

		QDFToken str{0,0};


		if (!valid())
		{
			// Trying to read and we can't!
			error = QDFParseError::UNEXPECTED_END_OF_FILE;
			return {0,0};
		}

		if (*cur == QDF_STRING_CONTAINER)
		{
			//Skip over container char and set start of string
			str.str = ++cur;
			
			// In string. Read until next string container
			for (; valid() && *cur != QDF_STRING_CONTAINER; cur++);
			
			if (*cur != QDF_STRING_CONTAINER)
			{
				// If we're not currently on a end of string container, that's a unclosed string. Let's error!
				error = QDFParseError::UNCLOSED_STRING;
				return {0, 0};
			}
			str.len = cur - str.str;

			// Skip over end of string
			cur++;
		}
		else if (!isControlCharacter(cur))
		{
			// Not a control character? Must be a quoteless string!
			str.str = cur;

			// Skip until control character or whitespace
			for (; valid() && !isControlCharacter(cur) && !isWhitespace(*cur); cur++);

			str.len = cur - str.str;
		}


		return str;
	}

	// Current character
	const char* cur;
	// Orignal string in
	const char* input;
	// Length of input
	size_t length;
	
	QDFParseError error;
};




////////////////
// QDF Parser //
////////////////

class ng::qdf::QDFParser
{
public:
	QDFParser(QDFRoot* root, const char* str, size_t length = SIZE_MAX)
	{
		stringBuffer = nullptr;
		stringArray = nullptr;
		qdfArray = nullptr;

		in = QDFInput(str, length);
		this->root = root;
		root->values.elements = nullptr;
		error = prospect();
		if (error != QDFParseError::NONE)
			return;

		// Allocate all of our resources
		root->qdfArray = qdfArrayPos = qdfArray = (QDF*)malloc(sizeof(QDF) * qdfCount);
		root->stringArray = stringArrayPos = stringArray = (QDF::String*)malloc(sizeof(QDF::String) * strCount);
		root->stringBuffer = stringBufferPos = stringBuffer = (char*)malloc(sizeof(char) * charCount);
		
		root->children.elements = qdfArrayPos;
		tokenList.resetHead();
		error = parse(root);
		if (error != QDFParseError::NONE)
			return;
	}

	void erase()
	{
		if (stringBuffer)
			free(stringBuffer);
		if (stringArray)
			free(stringArray);
		if (qdfArray)
			free(qdfArray);

	}

	void readString()
	{
		QDFToken str = in.readString();
		charCount += str.len + 1; // One extra for a zero at the end
		strCount++;
		*tokenList.add() = str;
	}

	// Light parse/tokenize and count
	QDFParseError prospect()
	{
		size_t strCount  = 0;

		QDFToken* parent = nullptr;

		for (; in.skip() && in.valid();)
		{
			// End of subblock?
			if (*in.cur == QDF_SUBBLOCK_END)
			{
				if (parent)
				{
					// Add token and skip over the end
					QDFToken* end = tokenList.add();
					*end = { in.cur++, 1, parent, parent->skip };
				
					parent->skip = tokenList.endLocation();
					parent = parent->parent;
				}
				else
				{
					return QDFParseError::UNEXPECTED_END_OF_SUBBLOCK;
				}
				continue;
			}

			// Get the key
			// Keys count towards the char count, but not towards the str count
			QDFToken key = in.readString();
			charCount += key.len + 1; // One extra for a zero at the end
			*tokenList.add() = key;
			if (in.error != QDFParseError::NONE) return in.error;


			// Read values

			// Is our value a list?
			if (in.skip() == QDF_LIST_BEGIN)
			{
				// Add start list token and skip over the char
				*tokenList.add() = { in.cur++, 1 };

				// Skip over the list begin char and loop until and end of list
				for (; in.valid() && in.skip() != QDF_LIST_END; )
				{
					readString();
					if (in.error != QDFParseError::NONE) return in.error;
				}

				if (*in.cur != QDF_LIST_END)
				{
					// If our cur isn't end, we broke due to being invalid. Syntax error!
					return QDFParseError::UNCLOSED_LIST;
				}

				// Add end list token and skip over the char
				*tokenList.add() = { in.cur++, 1 };
			}
			else if (*in.cur != QDF_SUBBLOCK_BEGIN)
			{
				// Not a list, so we just have one value
				readString();
				if (in.error != QDFParseError::NONE) return in.error;
			}

			// We've got one full qdf at this point. Up the count
			qdfCount++;

			// Read subblock
			if (in.skip() == QDF_SUBBLOCK_BEGIN)
			{
				QDFToken* begin = tokenList.add();
				// We store our loc so that our end can skip back to the start
				*begin = { in.cur++, 1, parent, tokenList.endLocation() };
				parent = begin;
			}

		}
		return QDFParseError::NONE;
	}
	QDFParseError parse(QDF* parent)
	{

		for (QDFToken* tok = tokenList.cur(); tok;)
		{
			// Are we ending our block?
			if (isChar(tok, QDF_SUBBLOCK_END))
			{
				if (parent == root)
				{
					// Parent isn't a subblock; it's root and this is a syntax error...
					return QDFParseError::UNEXPECTED_END_OF_SUBBLOCK;
				}

				auto loc = tokenList.location();
				for (size_t i = 0; i < parent->children.elementCount; i++)
				{
					QDF* q = &parent->children.elements[i];
					if (q->children.elements)
					{
						// Access our cheesy memory hack from earlier.
						tokenList.setLocation(*reinterpret_cast<QuickWriteList<QDFToken>::Location*>(q->children.elements));
						tokenList.next();

						q->children.elements = qdfArrayPos;
						parse(q);
					}
				}
				tokenList.setLocation(loc);
				tokenList.next();

				return QDFParseError::NONE;
			}

			if (!tok && parent != root)
				return QDFParseError::UNCLOSED_SUBBLOCK;

			QDF* qdf = qdfArrayPos++;

			// Read key
			qdf->key = copyInPlace(tok);

			tok = tokenList.next();
			if (!tok)
				return QDFParseError::UNEXPECTED_END_OF_FILE;

			// Read values
			
			// Is our value a list?
			if (isChar(tok, QDF_LIST_BEGIN))
			{
				qdf->values.elementCount = 0;
				qdf->values.elements = stringArrayPos;
				// Skip over the list begin char and loop until and end of list
				for (tok = tokenList.next(); tok && !isChar(tok, QDF_LIST_END); tok = tokenList.next())
				{
					*(stringArrayPos++) = copyInPlace(tok);
					qdf->values.elementCount++;
				}
				if (!tok || !isChar(tok, QDF_LIST_END))
				{
					return QDFParseError::UNCLOSED_LIST;

				}
				tok = tokenList.next();
			}
			else if(!isChar(tok, QDF_SUBBLOCK_BEGIN))
			{
				// Not a list, so we just have one value
				qdf->values.elementCount = 1;
				qdf->values.elements = stringArrayPos;
				*(stringArrayPos++) = copyInPlace(tok);
				tok = tokenList.next();
			}
			else
			{
				qdf->values.elementCount = 0;
				qdf->values.elements = nullptr;
			}
			// Read subblock
			qdf->children.elementCount = 0;
			qdf->children.elements = nullptr;
			if (tok)
			{
				if (isChar(tok, QDF_SUBBLOCK_BEGIN))
				{
					
					// Skip to after the subblock
					tokenList.setLocation(tok->skip);

					// Cheesy memory save hack.
					qdf->children.elements = reinterpret_cast<QDF*>(&tokenList.cur()->skip);

					tok = tokenList.next();


				}
			}
			else if (parent != root)
				return QDFParseError::UNCLOSED_SUBBLOCK;
			parent->children.elementCount++;
		}

		// If our parent wasn't root, and we got here, that's an unclosed subblock. Syntax error!
		if (parent != root)
			return QDFParseError::UNCLOSED_SUBBLOCK;

		for (size_t i = 0; i < parent->children.elementCount; i++)
		{
			QDF* q = &parent->children.elements[i];
			if (q->children.elements)
			{
				// Access our cheesy memory hack from earlier.
				tokenList.setLocation(*reinterpret_cast<QuickWriteList<QDFToken>::Location*>(q->children.elements));
				tokenList.next();
				q->children.elements = qdfArrayPos;
				parse(q);
			}
		}

		return QDFParseError::NONE;
	}
	
	QDF::String copyInPlace(QDFToken* str)
	{
		memcpy(stringBufferPos, str->str, str->len);
		str->str = stringBufferPos;
		stringBufferPos += str->len;
		*stringBufferPos = 0;
		stringBufferPos++;

		return { str->str, str->len };
	}

	QDFParseError error;
	QDFInput in;

	QDFRoot* root;
	
	QuickWriteList<QDFToken> tokenList = QuickWriteList<QDFToken>(8, 0, 2, 128);

	// Total count of chars in all strings
	size_t charCount;
	// Total count of all strings
	size_t strCount;
	// Total count of all qdfs
	size_t qdfCount;

	// Data building
	char* stringBuffer;
	char* stringBufferPos;
	QDF::String* stringArray;
	QDF::String* stringArrayPos;
	QDF* qdfArray;
	QDF* qdfArrayPos;
};


///////////////////
// QDF Root Node //
///////////////////

QDFRoot::QDFRoot()
{
	stringBuffer = 0;
	stringArray = 0;
	qdfArray = 0;
}

void QDFRoot::fromString(QDFParseError& error, const char* str, size_t length)
{
	// Don't allow copying on top of already existing data!
	if (stringBuffer || stringArray || qdfArray)
	{
		error = QDFParseError::DATA_ALREADY_PARSED;
		return;
	}

	QDFParser parser(this, str, length);
	error = parser.error;
	if (parser.error != QDFParseError::NONE)
	{
		// Failed to parse! cleanup time...
		parser.erase();

		stringBuffer = 0;
		stringArray = 0;
		qdfArray = 0;
	}
}

void QDFRoot::fromFile(QDFParseError& error, const char* path)
{
	FILE* f;

	// Windows loves to have special versions of things...
#ifdef _WIN32
	fopen_s(&f, path, "rb");
#else
	f = fopen(path, "rb");
#endif

	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	fseek(f, 0, 0);
	char* buf = (char*)calloc(len + 1, 1);
	fread(buf, 1, len, f);
	
	// Len isn't always exactly how long the string is, but it's a close estimate
	fromString(error, buf, len);

	free(buf);
}

QDFRoot::~QDFRoot()
{
	if (stringBuffer)
		free(stringBuffer);
	if (stringArray)
		free(stringArray);
	if (qdfArray)
		free(qdfArray);
}