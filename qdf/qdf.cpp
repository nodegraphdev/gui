#include "qdf.h"
#include "quickwritelist.h"

#include <stdint.h>
#include <string.h>
#include <iostream>

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
inline bool isControlCharacter(const char* c)
{
	char v = *c;
	return v == QDF_LIST_BEGIN
		|| v == QDF_LIST_END
		|| v == QDF_SUBBLOCK_BEGIN
		|| v == QDF_SUBBLOCK_END
		|| v == QDF_STRING_CONTAINER
		|| strncmp(c, QDF_COMMENT, sizeof(QDF_COMMENT) - 1) == 0
		|| strncmp(c, QDF_MULTILINE_COMMENT_BEGIN, sizeof(QDF_MULTILINE_COMMENT_BEGIN) - 1) == 0;
		// End of multiline is only considered for the actual comment
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

	QDF::String readString()
	{

		QDF::String str{0,0};


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


// Only difference is that this deletes all of the arrays associated with a qdf
class QDFRoot : public QDF
{
public:
	~QDFRoot()
	{
		if (stringBuffer)
			free(stringBuffer);
		if (stringArray)
			free(stringArray);
		if (qdfArray)
			free(qdfArray);
	}

	char* stringBuffer;
	QDF::String* stringArray;
	QDF* qdfArray;
};


////////////////
// QDF Parser //
////////////////

class QDFParser
{
public:
	QDFParser(const char* str, size_t length = SIZE_MAX)
	{
		stringBuffer = nullptr;
		stringArray = nullptr;
		qdfArray = nullptr;

		in = QDFInput(str, length);

		root = new QDFRoot();
		root->values = nullptr;
		error = parse(root, &qdfList);

		if (error == QDFParseError::NONE)
		{
			buildAllData();

			if (qdfArrayPos > qdfList.count() || stringArrayPos > strList.count() || stringBufferPos > charCount)
				error = QDFParseError::DATA_BUILD_ERROR;
		}

		if (error == QDFParseError::NONE)
		{
			// No error! Finalize by linking up our root
			root->stringArray = stringArray;
			root->stringBuffer = stringBuffer;
			root->qdfArray = qdfArray;
		}
	}

	void erase()
	{
		if (stringBuffer)
			free(stringBuffer);
		if (stringArray)
			free(stringArray);
		if (qdfArray)
			free(qdfArray);
		delete root;
	}

	void readString()
	{
		QDF::String str = in.readString();
		charCount += str.len + 1; // One extra for a zero at the end
		*strList.add() = str;
	}

	QDFParseError parse(QDF* parent, QuickWriteList<QDF>* qdfList)
	{
		// To attempt to order our data for less allocations, we're going to try to hold onto qdf lists until later
		QuickWriteList<QuickWriteList<QDF>> qdfWriteLists;

		for (; in.skip() && in.valid();)
		{

			// Are we ending our block?
			if (*in.cur == QDF_SUBBLOCK_END)
			{
				if (parent == root)
				{
					// Parent isn't a subblock; it's root and this is a syntax error...
					return QDFParseError::UNEXPECTED_END_OF_SUBBLOCK;
				}

				// Skip the end char
				in.cur++;

				// Before we can end, we have to tack on all of our qdfs
				for (auto wl = qdfWriteLists.begin(); wl; wl = qdfWriteLists.next())
				{
					qdfList->merge(*wl);
				}

				return QDFParseError::NONE;
			}

			// In this code, you'll see lots of references to just putting strings into a list.
			// Don't worry about it, we'll figure out where they belong later.

			QDF* qdf = qdfList->add();

			// Read key
			readString();
			if (in.error != QDFParseError::NONE) return in.error;

			// Read values

			// Is our value a list?
			if (in.skip() == QDF_LIST_BEGIN)
			{
				qdf->valueCount = 0;
				// Skip over the list begin char and loop until and end of list
				for (in.cur++; in.valid() && in.skip() != QDF_LIST_END; )
				{
					readString();
					if (in.error != QDFParseError::NONE) return in.error;
					qdf->valueCount++;
				}

				if (*in.cur != QDF_LIST_END)
				{
					// If our cur isn't end, we broke due to being invalid. Syntax error!
					return QDFParseError::UNCLOSED_LIST;
				}
				
				// Skip the end char
				in.cur++;
			}
			else
			{
				// Not a list, so we just have one value
				qdf->valueCount = 1;
				readString();
				if (in.error != QDFParseError::NONE) return in.error;
			}

			// Read subblock
			qdf->childCount = 0;
			if (in.skip() == QDF_SUBBLOCK_BEGIN)
			{
				// We've got a subblock. Skip a char and recursively parse
				in.cur++;
				parse(qdf, qdfWriteLists.add());
			}

			parent->childCount++;
		}

		// Before we can end, we have to tack on all of our qdfs
		for (auto wl = qdfWriteLists.begin(); wl; wl = qdfWriteLists.next())
		{
			qdfList->merge(*wl);
		}

		// If our parent wasn't root, and we got here, that's an unclosed subblock. Syntax error!
		if (parent == root)
			return QDFParseError::NONE;
		else
			return QDFParseError::UNCLOSED_SUBBLOCK;
	}
	
	void buildAllData()
	{
		// We've got a list of strings and counts. Now we need to compact down this data and link it up correctly...
		stringBuffer = (char*)malloc(charCount);
		stringBufferPos = 0;
		stringArray = strList.compact();
		qdfArray = qdfList.compact();

		buildData(root);
	}

	void buildData(QDF* qdf)
	{
		// Loop over our current pos in our arrays and link up our data
		qdf->children = qdfArray + qdfArrayPos;
		qdfArrayPos += qdf->childCount;
		
		if (qdf != root)
		{
			// Copy in strings
			qdf->key = copyInPlace(&stringArray[stringArrayPos++]);
			
			// Copy in values
			qdf->values = stringArray + stringArrayPos;
			stringArrayPos += qdf->valueCount;
			for (size_t i = 0; i < qdf->valueCount; i++)
				 copyInPlace(&qdf->values[i]);
		}

		for (size_t i = 0; i < qdf->childCount; i++)
		{
			buildData(&qdf->children[i]);
		}
	}

	QDF::String* copyInPlace(QDF::String* str)
	{
		memcpy(stringBuffer + stringBufferPos, str->str, str->len);
		str->str = stringBuffer + stringBufferPos;
		stringBufferPos += str->len;
		stringBuffer[stringBufferPos] = 0;
		stringBufferPos++;

		return str;
	}

	QDFParseError error;

	QDFRoot* root;
	
	QDFInput in;

	QuickWriteList<QDF::String> strList;
	QuickWriteList<QDF> qdfList;

	// Total count of chars in all strings
	size_t charCount;

	// Data building
	char* stringBuffer;
	size_t stringBufferPos;
	QDF::String* stringArray;
	size_t stringArrayPos;
	QDF* qdfArray;
	size_t qdfArrayPos;
};


QDF* QDF::parse(const char* str, QDFParseError& error)
{
	QDFParser parser(str);
	error = parser.error;
	if (parser.error == QDFParseError::NONE)
	{
		return parser.root;
	}
	else
	{
		// Failed to parse! cleanup time...
		parser.erase();
		return nullptr;
	}
}

QDF* QDF::parse(const char* str, size_t length, QDFParseError& error)
{
	QDFParser parser(str, length);
	error = parser.error;
	if (parser.error == QDFParseError::NONE)
	{
		return parser.root;
	}
	else
	{
		// Failed to parse! cleanup time...
		parser.erase();
		return nullptr;
	}
}

