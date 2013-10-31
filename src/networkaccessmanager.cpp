/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2011 Ariya Hidayat <ariya.hidayat@gmail.com>
  Copyright (C) 2011 Ivan De Marino <ivan.de.marino@gmail.com>

  Redistribution and use in source and binary forms, with or withou
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

#include <QAuthenticator>
#include <QDateTime>
#include <QDesktopServices>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslSocket>
#include <QSslCertificate>
#include <QRegExp>

#include "phantom.h"
#include "config.h"
#include "cookiejar.h"
#include "networkaccessmanager.h"
#include <string>
#include <string>
#include <sstream>
#include <map>
#include <fstream>
const qint64 MAX_REQUEST_POST_BODY_SIZE = 10 * 1000 * 1000;
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

void exitWithError1(const std::string &error) 
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
			exitWithError1("CFG: Can only have unique key names!\n");
	}

	void parseLine(const std::string &line, size_t const lineNo)
	{
		if (line.find('=') == line.npos)
		exitWithError1("CFG: Couldn't find separator on line: " + Convert::T_to_string(lineNo) + "\n");

		if (!validLine(line))
			exitWithError1("CFG: Bad format for line: " + Convert::T_to_string(lineNo) + "\n");

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




static const char *toString(QNetworkAccessManager::Operation op)
{
    const char *str = 0;
    switch (op) {
    case QNetworkAccessManager::HeadOperation:
        str = "HEAD";
        break;
    case QNetworkAccessManager::GetOperation:
        str = "GET";
        break;
    case QNetworkAccessManager::PutOperation:
        str = "PUT";
        break;
    case QNetworkAccessManager::PostOperation:
        str = "POST";
        break;
    case QNetworkAccessManager::DeleteOperation:
        str = "DELETE";
        break;
    default:
        str = "?";
        break;
    }
    return str;
}

TimeoutTimer::TimeoutTimer(QObject* parent)
    : QTimer(parent)
{
}


JsNetworkRequest::JsNetworkRequest(QNetworkRequest* request, QObject* parent)
    : QObject(parent)
{
    m_networkRequest = request;
}

void JsNetworkRequest::abort()
{
    if (m_networkRequest) {
        m_networkRequest->setUrl(QUrl());
    }
}

bool JsNetworkRequest::setHeader(const QString& name, const QVariant& value)
{
    if (!m_networkRequest)
        return false;

    // Pass `null` as the second argument to remove a HTTP header
    m_networkRequest->setRawHeader(name.toAscii(), value.toByteArray());
    return true;
}

void JsNetworkRequest::changeUrl(const QString& address)
{
    if (m_networkRequest) {
        QUrl url = QUrl::fromEncoded(QByteArray(address.toAscii()));
        m_networkRequest->setUrl(url);
    }
}

// public:
NetworkAccessManager::NetworkAccessManager(QObject *parent, const Config *config)
    : QNetworkAccessManager(parent)
    , m_ignoreSslErrors(config->ignoreSslErrors())
    , m_authAttempts(0)
    , m_maxAuthAttempts(3)
    , m_resourceTimeout(0)
    , m_idCounter(0)
    , m_networkDiskCache(0)
    , m_sslConfiguration(QSslConfiguration::defaultConfiguration())
{
    setCookieJar(CookieJar::instance());

    if (config->diskCacheEnabled()) {
        m_networkDiskCache = new QNetworkDiskCache(this);
        m_networkDiskCache->setCacheDirectory(QDesktopServices::storageLocation(QDesktopServices::CacheLocation));
        if (config->maxDiskCacheSize() >= 0)
            m_networkDiskCache->setMaximumCacheSize(config->maxDiskCacheSize() * 1024);
        setCache(m_networkDiskCache);
    }

    if (QSslSocket::supportsSsl()) {
        m_sslConfiguration = QSslConfiguration::defaultConfiguration();

        if (config->ignoreSslErrors()) {
            m_sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);
        }

        // set the SSL protocol to SSLv3 by the default
        m_sslConfiguration.setProtocol(QSsl::SslV3);

        if (config->sslProtocol() == "sslv2") {
            m_sslConfiguration.setProtocol(QSsl::SslV2);
        } else if (config->sslProtocol() == "tlsv1") {
            m_sslConfiguration.setProtocol(QSsl::TlsV1);
        } else if (config->sslProtocol() == "any") {
            m_sslConfiguration.setProtocol(QSsl::AnyProtocol);
        }

        if (!config->sslCertificatesPath().isEmpty()) {
          QList<QSslCertificate> caCerts = QSslCertificate::fromPath(
              config->sslCertificatesPath(), QSsl::Pem, QRegExp::Wildcard);

            m_sslConfiguration.setCaCertificates(caCerts);
        }
    }

    connect(this, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)), SLOT(provideAuthentication(QNetworkReply*,QAuthenticator*)));
    connect(this, SIGNAL(finished(QNetworkReply*)), SLOT(handleFinished(QNetworkReply*)));
}

void NetworkAccessManager::setUserName(const QString &userName)
{
    m_userName = userName;
}

void NetworkAccessManager::setPassword(const QString &password)
{
    m_password = password;
}

void NetworkAccessManager::setResourceTimeout(int resourceTimeout)
{
    m_resourceTimeout = resourceTimeout;
}

void NetworkAccessManager::setMaxAuthAttempts(int maxAttempts)
{
    m_maxAuthAttempts = maxAttempts;
}

void NetworkAccessManager::setCustomHeaders(const QVariantMap &headers)
{
    m_customHeaders = headers;
}

QVariantMap NetworkAccessManager::customHeaders() const
{
    return m_customHeaders;
}

void NetworkAccessManager::setCookieJar(QNetworkCookieJar *cookieJar)
{
    QNetworkAccessManager::setCookieJar(cookieJar);
    // Remove NetworkAccessManager's ownership of this CookieJar and
    // pass it to the PhantomJS Singleton object.
    // CookieJar is a SINGLETON, shouldn't be deleted when
    // the NetworkAccessManager is deleted but only when we shutdown.
    cookieJar->setParent(Phantom::instance());
}

// protected:

extern ConfigFile *cfg;
char s3[500];
char s4[500];
QNetworkReply *NetworkAccessManager::createRequest(Operation op, const QNetworkRequest & request, QIODevice * outgoingData)
{
    QNetworkRequest req(request);

    if (!QSslSocket::supportsSsl()) {
        if (req.url().scheme().toLower() == QLatin1String("https"))
            qWarning() << "Request using https scheme without SSL support";
    } else {
        req.setSslConfiguration(m_sslConfiguration);
    }
QByteArray tempurl = req.url().toEncoded();
std::string S1=tempurl.data();
printf("\n Unaltered URL\t --> %s",S1.c_str());
char *p=NULL;
if(strstr((char *)S1.c_str(),".com"))
{
p=strstr((char *)S1.c_str(),".com");
if(p+4 != NULL)
{p=p+4;}
std::string S2(p);
// char s3[100];
//char s4[100];

//printf("\nString S2 == %s",p);
//char temp1[0]={'\0'};
//strncpy(temp1,S2.c_str(),10);
//temp1[10]='\0';
//std::string S2=S1.substr(21);
for (std::map<std::string,std::string>::iterator it=cfg->contents.begin(); it!=cfg->contents.end(); ++it)
{
if(strstr(tempurl.data(),it->first.c_str()))
        {
	//char s3[100];
	//char s4[100];
	memset(s3,0,500);
	memset(s4,0,500);
	strcpy(s3,"http://");
	std::string S3="http://";
	std::string S4(it->second.c_str());
	int i=0;
	int count=0;
	for(i=0;i<it->second.length();i++)
	{
        	if(S4[i] == ',')
        	{
        	break;
        	}
		count++;
	}

	
	
	
	
	
//	printf("\nfrom MAp%d" ,strlen(it->second.c_str()));
	strncpy(s4,it->second.c_str(),count);
	//printf("\nLenght is -->%d",strlen(s3));
	//strcat(s3,it->second.c_str());
	strcat(s3,s4);
	S3.append(S4);
	S3.append(S2);
	//printf("\nS3 ---- > %s",s3);
	strcat(s3,S2.c_str());
	int temp=strlen(s3);
	//printf("\nLenght is -->%d",temp);
        //memcpy(s4,s3,strlen(s3)); 
	//s3[temp]='\0';
printf("\nS3 ----> %s ",s3);
        QUrl newUrl(s3);
	//printf("\nS3 ----> %s ",s3);
        req.setUrl(newUrl);
	break;
        }
}
}
    // Get the URL string before calling the superclass. Seems to work around
    // segfaults in Qt 4.8: https://gist.github.com/1430393
    QByteArray url = req.url().toEncoded();
    QByteArray postData;

    // http://code.google.com/p/phantomjs/issues/detail?id=337
    if (op == QNetworkAccessManager::PostOperation) {
        if (outgoingData) postData = outgoingData->peek(MAX_REQUEST_POST_BODY_SIZE);
        QString contentType = req.header(QNetworkRequest::ContentTypeHeader).toString();
        if (contentType.isEmpty()) {
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        }
    }

    // set custom HTTP headers
    QVariantMap::const_iterator i = m_customHeaders.begin();
    while (i != m_customHeaders.end()) {
        req.setRawHeader(i.key().toAscii(), i.value().toByteArray());
        ++i;
    }

    m_idCounter++;

    QVariantList headers;
    foreach (QByteArray headerName, req.rawHeaderList()) {
        QVariantMap header;
        header["name"] = QString::fromUtf8(headerName);
        header["value"] = QString::fromUtf8(req.rawHeader(headerName));
        headers += header;
    }

    QVariantMap data;
    data["id"] = m_idCounter;
    data["url"] = url.data();
printf("\n Url currelty looking up --> %s \n",url.data());
    data["method"] = toString(op);
    data["headers"] = headers;
    if (op == QNetworkAccessManager::PostOperation) data["postData"] = postData.data();
    data["time"] = QDateTime::currentDateTime();


//printf("\n Url currelty looking up --> %s \n",url.data());
    JsNetworkRequest jsNetworkRequest(&req, this);
    emit resourceRequested(data, &jsNetworkRequest);

    // Pass duty to the superclass - Nothing special to do here (yet?)
    QNetworkReply *reply = QNetworkAccessManager::createRequest(op, req, outgoingData);

    // reparent jsNetworkRequest to make sure that it will be destroyed with QNetworkReply
    jsNetworkRequest.setParent(reply);

    // If there is a timeout set, create a TimeoutTimer
    if(m_resourceTimeout > 0){

        TimeoutTimer *nt = new TimeoutTimer(reply);
        nt->reply = reply; // We need the reply object in order to abort it later on.
        nt->data = data;
        nt->setInterval(m_resourceTimeout);
        nt->setSingleShot(true);
        nt->start();

        connect(nt, SIGNAL(timeout()), this, SLOT(handleTimeout()));
    }

    m_ids[reply] = m_idCounter;

    connect(reply, SIGNAL(readyRead()), this, SLOT(handleStarted()));
    connect(reply, SIGNAL(sslErrors(const QList<QSslError> &)), this, SLOT(handleSslErrors(const QList<QSslError> &)));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(handleNetworkError()));

    return reply;
}

void NetworkAccessManager::handleTimeout()
{
    TimeoutTimer *nt = qobject_cast<TimeoutTimer*>(sender());

    if(!nt->reply)
        return;

    nt->data["errorCode"] = 408;
    nt->data["errorString"] = "Network timeout on resource.";

    emit resourceTimeout(nt->data);

    // Abort the reply that we attached to the Network Timeout
    nt->reply->abort();
}

void NetworkAccessManager::handleStarted()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    if (m_started.contains(reply))
        return;

    m_started += reply;
    
    QVariantList headers;
    foreach (QByteArray headerName, reply->rawHeaderList()) {
        QVariantMap header;
        header["name"] = QString::fromUtf8(headerName);
        header["value"] = QString::fromUtf8(reply->rawHeader(headerName));
        headers += header;
    }

    QVariantMap data;
    data["stage"] = "start";
    data["id"] = m_ids.value(reply);
    data["url"] = reply->url().toEncoded().data();
    data["status"] = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    data["statusText"] = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute);
    data["contentType"] = reply->header(QNetworkRequest::ContentTypeHeader);
    data["bodySize"] = reply->size();
    data["redirectURL"] = reply->header(QNetworkRequest::LocationHeader);
    data["headers"] = headers;
    data["time"] = QDateTime::currentDateTime();

    emit resourceReceived(data);
}

void NetworkAccessManager::handleFinished(QNetworkReply *reply)
{
    if (!m_ids.contains(reply))
        return;

    QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    QVariant statusText = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute);

    this->handleFinished(reply, status, statusText);
}

void NetworkAccessManager::provideAuthentication(QNetworkReply *reply, QAuthenticator *authenticator)
{
    if (m_authAttempts++ < m_maxAuthAttempts)
    {
        authenticator->setUser(m_userName);
        authenticator->setPassword(m_password);
    }
    else
    {
        m_authAttempts = 0;
        this->handleFinished(reply, 401, "Authorization Required");
        reply->close();
    }
}

void NetworkAccessManager::handleFinished(QNetworkReply *reply, const QVariant &status, const QVariant &statusText)
{
    QVariantList headers;
    foreach (QByteArray headerName, reply->rawHeaderList()) {
        QVariantMap header;
        header["name"] = QString::fromUtf8(headerName);
        header["value"] = QString::fromUtf8(reply->rawHeader(headerName));
        headers += header;
    }

    QVariantMap data;
    data["stage"] = "end";
    data["id"] = m_ids.value(reply);
    data["url"] = reply->url().toEncoded().data();
    data["status"] = status;
    data["statusText"] = statusText;
    data["contentType"] = reply->header(QNetworkRequest::ContentTypeHeader);
    data["redirectURL"] = reply->header(QNetworkRequest::LocationHeader);
    data["headers"] = headers;
    data["time"] = QDateTime::currentDateTime();

    m_ids.remove(reply);
    m_started.remove(reply);

    emit resourceReceived(data);
}

void NetworkAccessManager::handleSslErrors(const QList<QSslError> &errors)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    foreach (QSslError e, errors) {
        qDebug() << "Network - SSL Error:" << e;
    }

    if (m_ignoreSslErrors)
        reply->ignoreSslErrors();
}

void NetworkAccessManager::handleNetworkError()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    qDebug() << "Network - Resource request error:"
             << reply->error()
             << "(" << reply->errorString() << ")"
             << "URL:" << reply->url().toString();

    QVariantMap data;
    data["id"] = m_ids.value(reply);
    data["url"] = reply->url().toString();
    data["errorCode"] = reply->error();
    data["errorString"] = reply->errorString();

    emit resourceError(data);
}
