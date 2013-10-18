/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2011 Ariya Hidayat <ariya.hidayat@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "consts.h"
#include "utils.h"
#include "env.h"
#include "phantom.h"

#ifdef Q_OS_LINUX
#include "client/linux/handler/exception_handler.h"
#endif
#ifdef Q_OS_MAC
#include "client/mac/handler/exception_handler.h"
#endif

#include <QApplication>
#include <QSslSocket>

#if !defined(QT_SHARED) && !defined(QT_DLL)
#include <QtPlugin>
#include <string>
#include <sstream>
#include <map>
#include <fstream>
Q_IMPORT_PLUGIN(qcncodecs)
Q_IMPORT_PLUGIN(qjpcodecs)
Q_IMPORT_PLUGIN(qkrcodecs)
Q_IMPORT_PLUGIN(qtwcodecs)
#endif

#ifdef Q_OS_WIN32
using namespace google_breakpad;
static google_breakpad::ExceptionHandler* eh;
#if !defined(QT_SHARED) && !defined(QT_DLL)
Q_IMPORT_PLUGIN(qico)
#endif
#endif

#if QT_VERSION != QT_VERSION_CHECK(4, 8, 4)
#error Something is wrong with the setup. Please report to the mailing list!
#endif

class Convert
{
public:
	template <typename T>
	static std::string T_to_string(T const &val) 
	{
		std::ostringstream ostr;
		ostr << val;

		return ostr.str();
	}
		
	template <typename T>
	static T string_to_T(std::string const &val) 
	{
		std::istringstream istr(val);
		T returnVal;
		if (!(istr >> returnVal))
                   {
		exit(1);
		}
		return returnVal;
	}

	static std::string string_to_T(std::string const &val)
	{
		return val;
	}
};

void exitWithError(const std::string &error) 
{

	exit(EXIT_FAILURE);
}

class ConfigFile
{
public:
	std::map<std::string, std::string> contents;
	std::string fName;

	void removeComment(std::string &line) const
	{
		if (line.find(';') != line.npos)
			line.erase(line.find(';'));
	}

	bool onlyWhitespace(const std::string &line) const
	{
		return (line.find_first_not_of(' ') == line.npos);
	}
	bool validLine(const std::string &line) const
	{
		std::string temp = line;
		temp.erase(0, temp.find_first_not_of("\t "));
		if (temp[0] == '=')
			return false;

		for (size_t i = temp.find('=') + 1; i < temp.length(); i++)
			if (temp[i] != ' ')
				return true;

		return false;
	}

	void extractKey(std::string &key, size_t const &sepPos, const std::string &line) const
	{
		key = line.substr(0, sepPos);
		if (key.find('\t') != line.npos || key.find(' ') != line.npos)
			key.erase(key.find_first_of("\t "));
	}
	void extractValue(std::string &value, size_t const &sepPos, const std::string &line) const
	{
		value = line.substr(sepPos + 1);
		value.erase(0, value.find_first_not_of("\t "));
		value.erase(value.find_last_not_of("\t ") + 1);
	}

	void extractContents(const std::string &line) 
	{
		std::string temp = line;
		temp.erase(0, temp.find_first_not_of("\t "));
		size_t sepPos = temp.find('=');

		std::string key, value;
		extractKey(key, sepPos, temp);
		extractValue(value, sepPos, temp);

		if (!keyExists(key))
			contents.insert(std::pair<std::string, std::string>(key, value));
		else
			exitWithError("CFG: Can only have unique key names!\n");
	}

	void parseLine(const std::string &line, size_t const lineNo)
	{
		if (line.find('=') == line.npos)
			exitWithError("CFG: Couldn't find separator on line: " + Convert::T_to_string(lineNo) + "\n");

		if (!validLine(line))
			exitWithError("CFG: Bad format for line: " + Convert::T_to_string(lineNo) + "\n");

		extractContents(line);
	}

	void ExtractKeys()
	{
		std::ifstream file;
		file.open(fName.c_str());
		if (!file)
			{
			printf("File NF");
			exit(1);
			}
		std::string line;
		size_t lineNo = 0;
		while (std::getline(file, line))
		{
			lineNo++;
			std::string temp = line;

			if (temp.empty())
				continue;

			removeComment(temp);
			if (onlyWhitespace(temp))
				continue;

			parseLine(temp, lineNo);
		}

		file.close();
	}
public:
	ConfigFile(const std::string &fName)
	{
		this->fName = fName;
		ExtractKeys();
	}

	bool keyExists(const std::string &key) const
	{
		return contents.find(key) != contents.end();
	}

	template <typename ValueType>
	ValueType getValueOfKey(const std::string &key, ValueType const &defaultValue = ValueType()) const
	{
		if (!keyExists(key))
			return defaultValue;

	return Convert::string_to_T<ValueType>(contents.find(key)->second);
	}
};


ConfigFile *cfg;

int main(int argc, char** argv, const char** envp)
{

    // Setup Google Breakpad exception handler
#ifdef Q_OS_LINUX
    google_breakpad::ExceptionHandler eh("/tmp", NULL, Utils::exceptionHandler, NULL, true);
#endif
#ifdef Q_OS_MAC
    google_breakpad::ExceptionHandler eh("/tmp", NULL, Utils::exceptionHandler, NULL, true, NULL);
#endif
#ifdef Q_OS_WIN32
    // This is needed for CRT to not show dialog for invalid param
    // failures and instead let the code handle it.
    _CrtSetReportMode(_CRT_ASSERT, 0);

    DWORD cbBuffer = ExpandEnvironmentStrings(TEXT("%TEMP%"), NULL, 0);



    if (cbBuffer == 0) {
        eh = new ExceptionHandler(TEXT("."), NULL, Utils::exceptionHandler, NULL, ExceptionHandler::HANDLER_ALL);
    } else {
        LPWSTR szBuffer = reinterpret_cast<LPWSTR>(malloc(sizeof(TCHAR) * (cbBuffer + 1)));

        if (ExpandEnvironmentStrings(TEXT("%TEMP%"), szBuffer, cbBuffer + 1) > 0) {
            wstring lpDumpPath(szBuffer);
            eh = new ExceptionHandler(lpDumpPath, NULL, Utils::exceptionHandler, NULL, ExceptionHandler::HANDLER_ALL);
        }
        free(szBuffer);
    }
#endif

    QApplication app(argc, argv);

    app.setWindowIcon(QIcon(":/phantomjs-icon.png"));
    app.setApplicationName("PhantomJS");
    app.setOrganizationName("Ofi Labs");
    app.setOrganizationDomain("www.ofilabs.com");
    app.setApplicationVersion(PHANTOMJS_VERSION_STRING);

    // Prepare the "env" singleton using the environment variables
    Env::instance()->parse(envp);

    // Registering an alternative Message Handler
    qInstallMsgHandler(Utils::messageHandler);

#if defined(Q_OS_LINUX)
    if (QSslSocket::supportsSsl()) {
        // Don't perform on-demand loading of root certificates on Linux
        QSslSocket::addDefaultCaCertificates(QSslSocket::systemCaCertificates());
    }
#endif

    // Get the Phantom singleton
    Phantom *phantom = Phantom::instance();

    cfg=new ConfigFile("dns.cfg");

    // Start script execution
    if (phantom->execute()) {
        app.exec();
    }

    // End script execution: delete the phantom singleton and set execution return value
    int retVal = phantom->returnValue();
    delete phantom;
    return retVal;
}
