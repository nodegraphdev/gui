#pragma once
#include <cstdint>
#include <string_view>
#include "iterarray.h"

namespace ng::qdf{

	/* QDF File Parser
	 *  - Example of using this parser:
	 *		
	 *		QDFParseError error;
	 *		QDFRoot qdf;
	 *		qdf.fromFile(error, "my/cool/file.qdf");
	 *		for (QDF& kid : qdf.children)
	 *			std::cout << kid.key;
	 */

	enum class QDFParseError
	{
		NONE = 0,
		
		UNCLOSED_STRING,
		UNCLOSED_LIST,
		UNCLOSED_SUBBLOCK,
		UNCLOSED_COMMENT,

		UNEXPECTED_END_OF_FILE,
		UNEXPECTED_END_OF_SUBBLOCK,
		UNEXPECTED_END_OF_LIST,

		DATA_ALREADY_PARSED,
	};
	
	
	
	class QDF
	{
	public:

		typedef std::string_view String;
		
		String key;

		IterArray<QDF::String> values;
		IterArray<QDF> children;

	protected:
		// You shouldn't be creating qdfs by hand!
		QDF() {}
	};

	class QDFRoot : public QDF
	{
	public:
		QDFRoot();
		~QDFRoot();
		
		void fromString(QDFParseError& error, const char* str, size_t length = SIZE_MAX);
		void fromFile(QDFParseError& error, const char* path);
	
	private:
		char* stringBuffer;
		QDF::String* stringArray;
		QDF* qdfArray;
	};

};