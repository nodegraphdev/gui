#pragma once


namespace ng::qdf{

	enum class QDFParseError
	{
		NONE = 0,
		UNCLOSED_STRING,
		UNCLOSED_LIST,
		UNCLOSED_SUBBLOCK,
		UNCLOSED_COMMENT,
		UNEXPECTED_END_OF_FILE,
		UNEXPECTED_END_OF_SUBBLOCK,

		DATA_BUILD_ERROR,
	};
	
	class QDF
	{
	public:

		// Should this be here?
		struct String
		{
			const char* str;
			size_t len;
		};

		static QDF* parse(const char* str, QDFParseError& error);
		static QDF* parse(const char* str, size_t length, QDFParseError& error);

		// This exists so that the root can call its own delete function
		virtual ~QDF() {}

		String* key;

		size_t valueCount;
		String* values;

		QDF* children;
		size_t childCount;

	};


};