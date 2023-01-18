/*
	Copyright (C) 2009-2013 jakago

	This file is part of CaptureStream, the flv downloader for NHK radio
	language courses.

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>

#include "downloadthread.h"
#include "ui_mainwindow.h"
#include "downloadmanager.h"
#include "customizedialog.h"
#include "mainwindow.h"
#include "urldownloader.h"
#include "utility.h"
#include "mp3.h"
#include "qt4qt5.h"

#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QXmlQuery>
#include <QTextCodec>
#include <QTemporaryFile>
#include <QDateTime>
#include <QEventLoop>
#include <QRegExp>
#include <QScriptEngine>
#include <QTextStream>
#include <QDate>
#include <QLocale>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QtNetwork>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QJsonValue>

#ifdef QT4_QT5_WIN
#define TimeOut " -m 10000 "
#else
#define TimeOut " -m 10 "
#endif

#define ScrambleLength 14
#define FlvMinSize 100	// ストリーミングが存在しなかった場合は13バイトだが少し大きめに設定
#define OriginalFormat "ts"
#define FilterOption "-bsf:a aac_adtstoasc"
#define CancelCheckTimeOut 500	// msec
#define DebugLog(s) if ( ui->toolButton_detailed_message->isChecked() ) {emit information((s));}

//--------------------------------------------------------------------------------
QString DownloadThread::prefix = "https://www2.nhk.or.jp/gogaku/st/xml/";
QString DownloadThread::suffix = "listdataflv.xml";
QString DownloadThread::json_prefix = "https://www.nhk.or.jp/radioondemand/json/";

QString DownloadThread::prefix1 = "https://nhk-vh.akamaihd.net/i/gogaku-stream/mp4/";
QString DownloadThread::prefix2 = "https://nhks-vh.akamaihd.net/i/gogaku-stream/mp4/";
QString DownloadThread::prefix3 = "https://vod-stream.nhk.jp/gogaku-stream/mp4/";
//QString DownloadThread::prefix1 = "https://vod-stream.nhk.jp/radioondemand/r/";
QString DownloadThread::suffix1 = "/master.m3u8";
QString DownloadThread::suffix2 = ".mp4/master.m3u8";
QString DownloadThread::suffix3 = "/index.m3u8";

QString DownloadThread::flv_host = "flv.nhk.or.jp";
QString DownloadThread::flv_app = "ondemand/";
QString DownloadThread::flv_service_prefix = "mp4:flv/gogaku/streaming/mp4/";

QString DownloadThread::flvstreamer;
QString DownloadThread::ffmpeg;
QString DownloadThread::Xml_koza;
QString DownloadThread::test;
QString DownloadThread::scramble;
QString DownloadThread::optional1;
QString DownloadThread::optional2;
QString DownloadThread::optional3;
QString DownloadThread::optional4;
QString DownloadThread::optional5;
QString DownloadThread::optional6;
QString DownloadThread::optional7;
QString DownloadThread::optional8;
QString DownloadThread::opt_title1;
QString DownloadThread::opt_title2;
QString DownloadThread::opt_title3;
QString DownloadThread::opt_title4;
QString DownloadThread::opt_title5;
QString DownloadThread::opt_title6;
QString DownloadThread::opt_title7;
QString DownloadThread::opt_title8;
QStringList DownloadThread::malformed = (QStringList() << "3g2" << "3gp" << "m4a" << "mov");

QHash<QString, QString> DownloadThread::ffmpegHash;
QHash<QProcess::ProcessError, QString> DownloadThread::processError;

//--------------------------------------------------------------------------------

DownloadThread::DownloadThread( Ui::MainWindowClass* ui ) : isCanceled(false), failed1935(false) {
	this->ui = ui;
	if ( ffmpegHash.empty() ) {
		ffmpegHash["aac"] = "%1,-vn,-acodec,copy,%2";
		ffmpegHash["m4a"] = "%1,-id3v2_version,3,-metadata,title=%3,-metadata,artist=NHK,-metadata,album=%4,-metadata,date=%5,-metadata,genre=Speech,-vn,-bsf,aac_adtstoasc,-acodec,copy,%2";
		ffmpegHash["mp3-64k-S"] = "%1,-id3v2_version,3,-write_xing,0,-metadata,title=%3,-metadata,artist=NHK,-metadata,album=%4,-metadata,date=%5,-metadata,genre=Speech,-vn,-acodec:a,libmp3lame,-ab,64k,-ac,2,%2";
		ffmpegHash["mp3-128k-S"] = "%1,-id3v2_version,3,-write_xing,0,-metadata,title=%3,-metadata,artist=NHK,-metadata,album=%4,-metadata,date=%5,-metadata,genre=Speech,-vn,-acodec:a,libmp3lame,-ab,128k,-ac,2,%2";
		ffmpegHash["mp3-48k-S"] = "%1,-id3v2_version,3,-write_xing,0,-metadata,title=%3,-metadata,artist=NHK,-metadata,album=%4,-metadata,date=%5,-metadata,genre=Speech,-vn,-acodec:a,libmp3lame,-ab,48k,-ac,2,%2";
		ffmpegHash["mp3-40k-M"] = "%1,-id3v2_version,3,-write_xing,0,-metadata,title=%3,-metadata,artist=NHK,-metadata,album=%4,-metadata,date=%5,-metadata,genre=Speech,-vn,-acodec:a,libmp3lame,-ab,40k,-ar,32k,-ac,1,%2";
		ffmpegHash["mp3-32k-M"] = "%1,-id3v2_version,3,-write_xing,0,-metadata,title=%3,-metadata,artist=NHK,-metadata,album=%4,-metadata,date=%5,-metadata,genre=Speech,-vn,-acodec:a,libmp3lame,-ab,32k,-ar,32k,-ac,1,%2";
		ffmpegHash["mp3-24k-M"] = "%1,-id3v2_version,3,-write_xing,0,-metadata,title=%3,-metadata,artist=NHK,-metadata,album=%4,-metadata,date=%5,-metadata,genre=Speech,-vn,-acodec:a,libmp3lame,-ab,24k,-ar,22.05k,-ac,1,%2";
		ffmpegHash["mp3-16k-M"] = "%1,-id3v2_version,3,-write_xing,0,-metadata,title=%3,-metadata,artist=NHK,-metadata,album=%4,-metadata,date=%5,-metadata,genre=Speech,-vn,-acodec:a,libmp3lame,-ab,16k,-ar,22.05k,-ac,1,%2";
	}

	if ( processError.empty() ) {
		processError[QProcess::FailedToStart] = "FailedToStart";
		processError[QProcess::Crashed] = "Crashed";
		processError[QProcess::Timedout] = "Timedout";
		processError[QProcess::ReadError] = "ReadError";
		processError[QProcess::WriteError] = "WriteError";
		processError[QProcess::UnknownError] = "UnknownError";
	}
}

QStringList DownloadThread::getAttribute( QString url, QString attribute ) {
	const QString xmlUrl = "doc('" + url + "')/musicdata/music/" + attribute + "/string()";
	QStringList attributeList;
	QXmlQuery query;
	query.setQuery( xmlUrl );
	if ( query.isValid() )
		query.evaluateTo( &attributeList );
	return attributeList;
}

QStringList DownloadThread::getJsonData( QString url, QString attribute ) {
	QStringList attributeList;
	attributeList.clear() ;
    	QEventLoop eventLoop;
	QNetworkAccessManager mgr;
 	QObject::connect(&mgr, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
	const QString jsonUrl = json_prefix + url + "/bangumi_" + url + "_01.json";
	QUrl url_json( jsonUrl );
	QNetworkRequest req;
	req.setUrl(url_json);
	QNetworkReply *reply = mgr.get(req);
	eventLoop.exec(); // blocks stack until "finished()" has been called
	
	if (reply->error() == QNetworkReply::NoError) {
		QString strReply = (QString)reply->readAll();
		QJsonDocument jsonResponse = QJsonDocument::fromJson(strReply.toUtf8());
		QJsonObject jsonObject = jsonResponse.object();
		QJsonObject jsonObj = jsonResponse.object();
    
		QJsonArray jsonArray = jsonObject[ "main" ].toArray();
		QJsonObject objx2 = jsonObject[ "main" ].toObject();
		QString program_name = objx2[ "program_name" ].toString().replace( "　", " " );
		    for (ushort i = 0xFF1A; i < 0xFF5F; ++i) {
		        program_name = program_name.replace(QChar(i), QChar(i - 0xFEE0));
		    }
		    for (ushort i = 0xFF10; i < 0xFF1A; ++i) {
		        program_name = program_name.replace( QChar(i - 0xFEE0), QChar(i) );
		    }
		QJsonArray detail_list2 = objx2[ "detail_list" ].toArray();
		QJsonArray detail_list = objx2[ "detail_list" ].toArray();

		foreach (const QJsonValue & value, detail_list) {
			QJsonObject objxx = value.toObject();
			QJsonArray file_list = objxx[ "file_list" ].toArray();					
			foreach (const QJsonValue & value, file_list) {
				QJsonObject objxx2 = value.toObject();
				QString file_title = objxx2[ "file_title" ].toString();
				QString file_name = objxx2[ "file_name" ].toString();
				QString onair_date = objxx2[ "onair_date" ].toString();
				QString open_time = objxx2[ "open_time" ].toString();
				QString year = open_time.left( 4 );				
					if ( attribute == "program_name" ) attributeList += program_name;
					if ( attribute == "file_title" ) attributeList += file_title;
					if ( attribute == "file_name" ) attributeList += file_name;
					if ( attribute == "onair_date" ) attributeList += onair_date;
					if ( attribute == "open_time" ) attributeList += year;
//                             emit critical( QString::fromUtf8( "file_title：(%1) " )
//					.arg( file_title ) );
//                            emit critical( QString::fromUtf8( "file_name：(%1) " )
//					.arg( file_name ) );
//                             emit critical( QString::fromUtf8( "onair_date：(%1) " )
//					.arg( onair_date ) );
        		}					
		}
	}
	return attributeList;
}

bool DownloadThread::checkExecutable( QString path ) {
	QFileInfo fileInfo( path );
	
	if ( !fileInfo.exists() ) {
		emit critical( path + QString::fromUtf8( "が見つかりません。" ) );
		return false;
	} else if ( !fileInfo.isExecutable() ) {
		emit critical( path + QString::fromUtf8( "は実行可能ではありません。" ) );
		return false;
	}
	
	return true;
}

bool DownloadThread::isFfmpegAvailable( QString& path ) {
	path = Utility::applicationBundlePath() + "ffmpeg";
#ifdef QT4_QT5_WIN
	path += ".exe";
#endif
	return checkExecutable( path );
}


//通常ファイルが存在する場合のチェックのために末尾にセパレータはついていないこと
bool DownloadThread::checkOutputDir( QString dirPath ) {
	bool result = false;
	QFileInfo dirInfo( dirPath );

	if ( dirInfo.exists() ) {
		if ( !dirInfo.isDir() )
			emit critical( QString::fromUtf8( "「" ) + dirPath + QString::fromUtf8( "」が存在しますが、フォルダではありません。" ) );
		else if ( !dirInfo.isWritable() )
			emit critical( QString::fromUtf8( "「" ) + dirPath + QString::fromUtf8( "」フォルダが書き込み可能ではありません。" ) );
		else
			result = true;
	} else {
		QDir dir;
		if ( !dir.mkpath( dirPath ) )
			emit critical( QString::fromUtf8( "「" ) + dirPath + QString::fromUtf8( "」フォルダの作成に失敗しました。" ) );
		else
			result = true;
	}

	return result;
}

//--------------------------------------------------------------------------------

QStringList DownloadThread::getElements( QString url, QString path ) {
	const QString xmlUrl = "doc('" + url + "')" + path;
	QStringList elementList;
	QXmlQuery query;
	query.setQuery( xmlUrl );
	if ( query.isValid() )
		query.evaluateTo( &elementList );
	return elementList;
}

//--------------------------------------------------------------------------------

QStringList one2two( QStringList hdateList ) {
	QStringList result;
	QRegExp rx("(\\d+)(?:\\D+)(\\d+)");

	for ( int i = 0; i < hdateList.count(); i++ ) {
		QString hdate = hdateList[i];
		if ( rx.indexIn( hdate, 0 ) != -1 ) {
			uint length = rx.cap( 2 ).length();
			if ( length == 1 )
				hdate.replace( rx.pos( 2 ), 1, "0" + rx.cap( 2 ) );
			length = rx.cap( 1 ).length();
			if ( length == 1 )
				hdate.replace( rx.pos( 1 ), 1, "0" + rx.cap( 1 ) );
		}
		result << hdate;
	}

	return result;
}

QStringList one2two2( QStringList hdateList2 ) {
	QStringList result;
	QRegExp rx("(\\d+)(?:\\D+)(\\d+)");

	for ( int i = 0; i < hdateList2.count(); i++ ) {
		QString hdate = hdateList2[i];
		if ( rx.indexIn( hdate, 0 ) != -1 ) {
			uint length = rx.cap( 2 ).length();
			if ( length == 1 )
				hdate.replace( rx.pos( 2 ), 1, "0" + rx.cap( 2 ) );
			length = rx.cap( 1 ).length();
			if ( length == 1 )
				hdate.replace( rx.pos( 1 ), 1, "0" + rx.cap( 1 ) );
		}
		QString month2 = hdate.left( 2 );
		QString day2 = hdate.mid( 3, 2 );
		QDate today;
		today.setDate(QDate::currentDate().year(), QDate::currentDate().month(), QDate::currentDate().day());
		QDateTime dt = QDateTime::fromString( month2 + "/" + day2, "MM/dd" ).addDays(7);
	
		QString str1 = dt.toString("MM");
		QString str2 = dt.toString("dd");

		hdate.replace( day2, str2 );
		hdate.replace( month2, str1 );

		result << hdate;
	}

	return result;
}

QStringList thisweekfile( QStringList fileList2, QStringList codeList ) {
	QStringList result;
	
	for ( int i = 0; i < fileList2.count(); i++ ) {
		QString filex = fileList2[i];
		int filexxx = codeList[i].toInt() + fileList2.count() ;
		filex.replace( codeList[i].right( 3 ) ,  QString::number( filexxx ).right( 3 ) );
		filex.remove( "-re01" );
			
		result << filex;
	}

	return result;
}

//--------------------------------------------------------------------------------

bool illegal( char c ) {
	bool result = false;
	switch ( c ) {
	case '/':
	case '\\':
	case ':':
	case '*':
	case '?':
	case '"':
	case '<':
	case '>':
	case '|':
	case '#':
	case '{':
	case '}':
	case '%':
	case '&':
	case '~':
		result = true;
		break;
	default:
		break;
	}
	return result;
}

QString DownloadThread::formatName( QString format, QString kouza, QString hdate, QString file, QString nendo, bool checkIllegal ) {
	int month = hdate.left( 2 ).toInt();
	int year = nendo.right( 4 ).toInt();
	int day = hdate.mid( 3, 2 ).toInt();
	int year1 = QDate::currentDate().year();

	if ( QString::compare(  kouza , QString::fromUtf8( "ボキャブライダー" ) ) ==0 ){
		if ( month == 3 && ( day == 30 || day == 31) && year == 2022 ) 
		year += 0;
 		else
		if ( month < 4 )
		year += 1;
	} else {
	if ( month <= 4 && QDate::currentDate().year() > year )
		year = year + (year1 - year);
	}

	if ( file.right( 4 ) == ".flv" )
		file = file.left( file.length() - 4 );

	QString result;

	bool percent = false;
	for ( int i = 0; i < format.length(); i++ ) {
		QChar qchar = format[i];
		if ( percent ) {
			percent = false;
			char ascii = qchar.toLatin1();
			if ( checkIllegal && illegal( ascii ) )
				continue;
			switch ( ascii ) {
			case 'k': result += kouza; break;
			case 'h': result += hdate.left( 6 ) + QString::fromUtf8( "放送分" ); break;
			case 'f': result += file; break;
			//case 'r': result += MainWindow::applicationDirPath(); break;
			//case 'p': result += QDir::separator(); break;
			case 'Y': result += QString::number( year ); break;
			case 'y': result += QString::number( year ).right( 2 ); break;
			case 'N': result += nendo + QString::fromUtf8( "年度" ); break;
			case 'n': result += nendo.right( 2 ) + QString::fromUtf8( "年度" ); break;
			case 'M': result += QString::number( month + 100 ).right( 2 ); break;
			case 'm': result += QString::number( month ); break;
			case 'D': result += QString::number( day + 100 ).right( 2 ); break;
			case 'd': result += QString::number( day ); break;
			default: result += qchar; break;
			}
		} else {
			if ( qchar == QChar( '%' ) )
				percent = true;
			else if ( checkIllegal && illegal( qchar.toLatin1() ) )
				continue;
			else
				result += qchar;
		}
	}

	return result;
}

//--------------------------------------------------------------------------------

bool DownloadThread::captureStream( QString kouza, QString hdate, QString file, QString nendo, QString dir, QString this_week ) {
	QString outputDir = MainWindow::outputDir + kouza;
	if ( this_week == "R" )
		outputDir = MainWindow::outputDir + QString::fromUtf8( "[前週]" )+ "/" + kouza;

	if ( !checkOutputDir( outputDir ) )
		return false;
	outputDir += QDir::separator();	//通常ファイルが存在する場合のチェックのために後から追加する

	QString titleFormat;
	QString fileNameFormat;
	CustomizeDialog::formats( "xml", titleFormat, fileNameFormat );
	QString id3tagTitle = formatName( titleFormat, kouza, hdate, file, nendo, false );
	QString outFileName = formatName( fileNameFormat, kouza, hdate, file, nendo, true );
	QFileInfo fileInfo( outFileName );
	QString outBasename = fileInfo.completeBaseName();
	
	// 2013/04/05 オーディオフォーマットの変更に伴って拡張子の指定に対応
	QString extension = ui->comboBox_extension->currentText();
	QString extension1 = extension;
	if ( extension.left( 3 ) == "mp3" ) extension1 = "mp3";
	outFileName = outBasename + "." + extension1;

#ifdef QT4_QT5_WIN
	QString null( "nul" );
#else
	QString null( "/dev/null" );
#endif
	int month = hdate.left( 2 ).toInt();
	int year = nendo.right( 4 ).toInt();
	int day = hdate.mid( 3, 2 ).toInt();
	if ( 2022 > year ) return false;
	int year1 = QDate::currentDate().year();

//	if ( QString::compare(  kouza , QString::fromUtf8( "ボキャブライダー" ) ) ==0 ){
//		if ( month == 3 && ( day == 30 || day == 31) && year == 2022 ) 
//		year += 0; 
 //		else
//		if ( month < 4 )
//		year += 1;
//	} else {
	if ( month <= 4 && QDate::currentDate().year() > year )
		year = year + (year1 - year);
//	}
	QDate onair( year, month, day );
	QString yyyymmdd = onair.toString( "yyyy_MM_dd" );

	QString kon_nendo = "2022"; //QString::number(year1);

	if ( ui->toolButton_skip->isChecked() && QFile::exists( outputDir + outFileName ) ) {
	   if ( this_week == "R" ) {
		emit current( QString::fromUtf8( "スキップ：[前週]　　" ) + kouza + QString::fromUtf8( "　" ) + yyyymmdd );
	   } else {
		emit current( QString::fromUtf8( "スキップ：　　　　　" ) + kouza + QString::fromUtf8( "　" ) + yyyymmdd );
	   }
		return true;
	}
	
	if ( this_week == "R" ) {
	  	emit current( QString::fromUtf8( "レコーディング中：[前週] " ) + kouza + QString::fromUtf8( "　" ) + yyyymmdd );
	} else {
  		emit current( QString::fromUtf8( "レコーディング中：　　" ) + kouza + QString::fromUtf8( "　" ) + yyyymmdd );
	}
	
	Q_ASSERT( ffmpegHash.contains( extension ) );
	QString dstPath;
#ifdef QT4_QT5_WIN
	if ( true ) {
		QTemporaryFile file;
		if ( file.open() ) {
			dstPath = file.fileName() + "." + extension1;
			file.close();
		} else {
			emit critical( QString::fromUtf8( "一時ファイルの作成に失敗しました：　" ) + kouza + QString::fromUtf8( "　" ) + yyyymmdd );
			return false;
		}
	}
#else
	dstPath = outputDir + outFileName;
#endif
	QString filem3u8a; QString filem3u8b; QString prefix1a = prefix1;  QString prefix2a = prefix2;  QString prefix3a = prefix3;
	if ( dir ==  ""  ) { prefix1a.remove("/mp4");        prefix2a.remove("/mp4");        prefix3a.remove("/mp4");
	} else             { prefix1a.replace( "mp4", dir ); prefix2a.replace( "mp4", dir ); prefix3a.replace( "mp4", dir ); }; 
	if ( file.right(4) != ".mp4" ) {
		filem3u8a = prefix1a + file + ".mp4/master.m3u8";
		filem3u8b = prefix2a + file + ".mp4/master.m3u8";
	} else {
		filem3u8a = prefix1a + file + "/master.m3u8";
		filem3u8b = prefix2a + file + "/master.m3u8";
	}
	QString filem3u8c = prefix3a + file  + "/index.m3u8";
	QStringList arguments_v = { "-http_seekable", "0", "-version", "0" };
	QProcess process_v;
	process_v.setProgram( ffmpeg );
	process_v.setArguments( arguments_v );
	process_v.start();
	process_v.waitForFinished();
	QString str_v = process_v.readAllStandardError();
	process_v.kill();
	process_v.close();	 
	QString arguments00 = "-y -http_seekable 0 -i";
	if (str_v.contains( "Option not found" )) {
	                     arguments00 = "-y -i";
	}

	QStringList arguments0 = arguments00.split(" ");
	QStringList arguments = arguments0 + ffmpegHash[extension]
			.arg( filem3u8a, dstPath, id3tagTitle, kouza, QString::number( year ) ).split(",");
	QStringList arguments2 = arguments0 + ffmpegHash[extension]
			.arg( filem3u8b, dstPath, id3tagTitle, kouza, QString::number( year ) ).split(","); 
	QStringList arguments3 = arguments0 + ffmpegHash[extension]
			.arg( filem3u8c, dstPath, id3tagTitle, kouza, QString::number( year ) ).split(","); 

	//qDebug() << commandFfmpeg;
	//DebugLog( commandFfmpeg );
	QProcess process;
	process.setProgram( ffmpeg );
	process.setArguments( arguments );
	
	QProcess process2;
	process2.setProgram( ffmpeg );
	process2.setArguments( arguments2 );
	
	QProcess process3;
	process3.setProgram( ffmpeg );
	process3.setArguments( arguments3 );
	
	process.start();
//	process.start( commandFfmpeg );
	if ( !process.waitForStarted( -1 ) ) {
		emit critical( QString::fromUtf8( "ffmpeg起動エラー(%3)：　%1　　%2" )
				.arg( kouza, yyyymmdd,  processError[process.error()] ) );
		QFile::remove( dstPath );
		return false;
	}

	// ユーザのキャンセルを確認しながらffmpegの終了を待つ
	while ( !process.waitForFinished( CancelCheckTimeOut ) ) {
		// キャンセルボタンが押されていたらffmpegをkillし、ファイルを削除してリターン
		if ( isCanceled ) {
			process.kill();
			QFile::remove( dstPath );
			return false;
		}
		// 単なるタイムアウトは継続
		if ( process.error() == QProcess::Timedout )
			continue;
		// エラー発生時はメッセージを表示し、出力ファイルを削除してリターン
		emit critical( QString::fromUtf8( "ffmpeg実行エラー(%3)：　%1　　%2" )
				.arg( kouza, yyyymmdd,  processError[process.error()] ) );
		QFile::remove( dstPath );
		return false;
	}


	QString ffmpeg_Error;
	ffmpeg_Error.append(process.readAllStandardError());

	// ffmpeg終了ステータスに応じた処理をしてリターン
	if ( process.exitCode() || ffmpeg_Error.contains("HTTP error") || ffmpeg_Error.contains("Unable to open resource:") ) {
//	if ( process.exitCode()  ) {
	process.kill();
	process.close();
	process2.start();

		if ( !process2.waitForStarted( -1 ) ) {
			emit critical( QString::fromUtf8( "ffmpeg起動エラー2(%3)：　%1　　%2" )
					.arg( kouza, yyyymmdd,  processError[process2.error()] ) );
			QFile::remove( dstPath );
			return false;
		}

	// ユーザのキャンセルを確認しながらffmpegの終了を待つ
		while ( !process2.waitForFinished( CancelCheckTimeOut ) ) {
		// キャンセルボタンが押されていたらffmpegをkillし、ファイルを削除してリターン
			if ( isCanceled ) {
				process2.kill();
				QFile::remove( dstPath );
				return false;
			}
		// 単なるタイムアウトは継続
			if ( process2.error() == QProcess::Timedout )
				continue;
		// エラー発生時はメッセージを表示し、出力ファイルを削除してリターン
			emit critical( QString::fromUtf8( "ffmpeg実行エラー2(%3)：　%1　　%2" )
					.arg( kouza, yyyymmdd,  processError[process2.error()] ) );
			QFile::remove( dstPath );
			return false;
		}

	QString ffmpeg_Error2;
	ffmpeg_Error2.append(process2.readAllStandardError());

	// ffmpeg終了ステータスに応じた処理をしてリターン
	if ( process2.exitCode() || ffmpeg_Error2.contains("HTTP error") || ffmpeg_Error2.contains("Unable to open resource:") ) {
//	if ( process2.exitCode()  ) {
	process2.kill();
	process2.close();
	process3.start();

		if ( !process3.waitForStarted( -1 ) ) {
			emit critical( QString::fromUtf8( "ffmpeg起動エラー3(%3)：　%1　　%2" )
					.arg( kouza, yyyymmdd,  processError[process3.error()] ) );
			QFile::remove( dstPath );
			return false;
		}

	// ユーザのキャンセルを確認しながらffmpegの終了を待つ
		while ( !process3.waitForFinished( CancelCheckTimeOut ) ) {
		// キャンセルボタンが押されていたらffmpegをkillし、ファイルを削除してリターン
			if ( isCanceled ) {
				process3.kill();
				QFile::remove( dstPath );
				return false;
			}
		// 単なるタイムアウトは継続
			if ( process3.error() == QProcess::Timedout )
				continue;
		// エラー発生時はメッセージを表示し、出力ファイルを削除してリターン
			emit critical( QString::fromUtf8( "ffmpeg実行エラー(%3)：　%1　　%2" )
					.arg( kouza, yyyymmdd,  processError[process3.error()] ) );
			QFile::remove( dstPath );
			return false;
		}
	
	QString ffmpeg_Error3;
	ffmpeg_Error3.append(process3.readAllStandardError());
	
	// ffmpeg終了ステータスに応じた処理をしてリターン
	if ( process3.exitCode() || ffmpeg_Error3.contains("HTTP error") || ffmpeg_Error3.contains("Unable to open resource:") ) {	
				emit critical( QString::fromUtf8( "レコーディング失敗：　%1　　%2" ).arg( kouza, yyyymmdd ) );
			QFile::remove( dstPath );
			return false;
		}
	}}
#ifdef QT4_QT5_WIN
		QFile::rename( dstPath, outputDir + outFileName );
#endif
			return true;
}



bool DownloadThread::captureStream_json( QString kouza, QString hdate, QString file, QString nendo, QString title ) {
	QString outputDir = MainWindow::outputDir + kouza;

	if ( !checkOutputDir( outputDir ) )
		return false;
	outputDir += QDir::separator();	//通常ファイルが存在する場合のチェックのために後から追加する

	QString titleFormat;
	QString fileNameFormat;
	CustomizeDialog::formats( "json", titleFormat, fileNameFormat );
//	QString id3tagTitle = title;
	QString id3tagTitle = formatName( titleFormat, kouza, hdate, title, nendo, false );
	QString outFileName = formatName( fileNameFormat, kouza, hdate, title, nendo, true );
	QFileInfo fileInfo( outFileName );
	QString outBasename = fileInfo.completeBaseName();
	
	// 2013/04/05 オーディオフォーマットの変更に伴って拡張子の指定に対応
	QString extension = ui->comboBox_extension->currentText();
	QString extension1 = extension;
	if ( extension.left( 3 ) == "mp3" ) extension1 = "mp3";
	outFileName = outBasename + "." + extension1;

#ifdef QT4_QT5_WIN
	QString null( "nul" );
#else
	QString null( "/dev/null" );
#endif
	int month = hdate.left( 2 ).toInt();
	int year = nendo.right( 4 ).toInt();
	int day = hdate.mid( 3, 2 ).toInt();
	if ( 2022 > year ) return false;
	int year1 = QDate::currentDate().year();

	if ( month <= 4 && QDate::currentDate().year() > year )
		year = year + (year1 - year);

	QDate onair( year, month, day );
	QString yyyymmdd = onair.toString( "yyyy_MM_dd" );

	QString kon_nendo = "2022"; //QString::number(year1);
	
	if ( ui->toolButton_skip->isChecked() && QFile::exists( outputDir + outFileName ) ) {
		emit current( QString::fromUtf8( "スキップ：　　　　　" ) + kouza + QString::fromUtf8( "　" ) + yyyymmdd );
	   	return true;
	}
  		emit current( QString::fromUtf8( "レコーディング中：　" ) + kouza + QString::fromUtf8( "　" ) + yyyymmdd );
	
	Q_ASSERT( ffmpegHash.contains( extension ) );
	QString dstPath;
#ifdef QT4_QT5_WIN
	if ( true ) {
		QTemporaryFile file;
		if ( file.open() ) {
			dstPath = file.fileName() + "." + extension1;
			file.close();
		} else {
			emit critical( QString::fromUtf8( "一時ファイルの作成に失敗しました：　" ) + kouza + QString::fromUtf8( "　" ) + yyyymmdd );
			return false;
		}
	}
#else
	dstPath = outputDir + outFileName;
#endif

	QStringList arguments_v = { "-http_seekable", "0", "-version", "0" };
	QProcess process_v;
	process_v.setProgram( ffmpeg );
	process_v.setArguments( arguments_v );
	process_v.start();
	process_v.waitForFinished();
	QString str_v = process_v.readAllStandardError();
	process_v.kill();
	process_v.close();	 
	QString arguments00 = "-y -http_seekable 0 -i";
	if (str_v.contains( "Option not found" )) {
	                     arguments00 = "-y -i";
	}
				
	QStringList arguments0 = arguments00.split(" ");
	QString filem3u8aA = file;
	QString id3tagTitleA = id3tagTitle;
	QString kouzaA = kouza;	
	
	QStringList argumentsA = arguments0 + ffmpegHash[extension]
			.arg( filem3u8aA, dstPath, id3tagTitleA, kouzaA,  nendo ).split(",");
	QProcess process;
	process.setProgram( ffmpeg );
	process.setArguments( argumentsA );
	process.start();

	if ( !process.waitForStarted( -1 ) ) {
		emit critical( QString::fromUtf8( "ffmpeg起動エラー(%3)：　%1　　%2" )
				.arg( kouza, yyyymmdd,  processError[process.error()] ) );
		QFile::remove( dstPath );
		return false;
	}

	// ユーザのキャンセルを確認しながらffmpegの終了を待つ
		while ( !process.waitForFinished( CancelCheckTimeOut ) ) {
		// キャンセルボタンが押されていたらffmpegをkillし、ファイルを削除してリターン
			if ( isCanceled ) {
				process.kill();
				QFile::remove( dstPath );
				return false;
			}
		// 単なるタイムアウトは継続
			if ( process.error() == QProcess::Timedout )
				continue;
		// エラー発生時はメッセージを表示し、出力ファイルを削除してリターン
			emit critical( QString::fromUtf8( "ffmpeg実行エラー(%3)：　%1　　%2" )
					.arg( kouza, yyyymmdd,  processError[process.error()] ) );
			QFile::remove( dstPath );
			return false;
		}

	// ffmpeg終了ステータスに応じた処理をしてリターン
		if ( process.exitCode() ) {
			emit critical( QString::fromUtf8( "レコーディング失敗：　%1　　%2" ).arg( kouza, yyyymmdd ) );
			QFile::remove( dstPath );
			return false;
		}

		QString ffmpeg_Error;
		 ffmpeg_Error.append(process.readAllStandardError());
				
		if ( ffmpeg_Error.contains("HTTP error") ) 
			emit critical( QString::fromUtf8( "HTTP error" ));
		if ( ffmpeg_Error.contains("Unable to open resource:") ) 
			emit critical( QString::fromUtf8( "Unable to open resource:" ));
				
		if ( ffmpeg_Error.contains("HTTP error") || ffmpeg_Error.contains("Unable to open resource:") || ffmpeg_Error.contains("parse_playlist error") ) {
			emit critical( QString::fromUtf8( "レコーディング失敗：　%1　　%2" ).arg( kouza, yyyymmdd ) );
			QFile::remove( dstPath );
			return false;
		}


	// ffmpeg終了ステータスに応じた処理をしてリターン
	if ( process.exitCode() ) {
	process.kill();
	process.close();
	}
#ifdef QT4_QT5_WIN
		QFile::rename( dstPath, outputDir + outFileName );
#endif
			return true;
}




QString DownloadThread::paths[] = {
	"english/basic0", "english/basic1", "english/basic2", "english/basic3",
	"english/timetrial",  "english/enjoy", "english/kaiwa", "english/business1",
	"null", "english/vr-radio",
//	"english/business2", "english/everybody", "english/gendai", "english/enjoy", 
	"null_optional1", "null_optional2", "null_optional3", "null_optional4",
	"null_optional5", "null_optional6", "null_optional7", "null_optional8"
};

QString DownloadThread::json_paths[] = {
	"0000", "6806", "6807", "6808",
	"2331", "3064", "0916", "6809", 
	"7512", "4121",
	"0953", "0943", "0946", "0948",
	"0953", "0943", "0946", "0948"
};


QString DownloadThread::paths2[] = {
	"english/basic0", "english/basic1", "english/basic2", "english/basic3",
	"english/timetrial", "english/kaiwa", "english/business1", "english/enjoy",
	"french/kouza", "french/kouza2", "german/kouza", "german/kouza2",
	"spanish/kouza", "spanish/kouza2", "italian/kouza", "italian/kouza2",
	"russian/kouza","russian/kouza2", "chinese/kouza", "chinese/stepup",
	"hangeul/kouza", "hangeul/stepup", 
	NULL
};

QString DownloadThread::json_paths2[] = { 
	"0000", "6806", "6807", "6808",
	"2331", "0916", "6809", "3064", 
	"0953", "4412", "0943", "4410",
	"0948", "4413", "0946", "4411",
	"0956", "4414", "0915", "6581",
	"0951", "6810", 
	NULL
};


void DownloadThread::run() {
	QAbstractButton* checkbox[] = {
		ui->toolButton_basic0, ui->toolButton_basic1, ui->toolButton_basic2, ui->toolButton_basic3,
		ui->toolButton_timetrial, ui->toolButton_enjoy, ui->toolButton_kaiwa, ui->toolButton_business1,
		ui->toolButton_gendai, ui->toolButton_vrradio,
		ui->toolButton_optional1, ui->toolButton_optional2, 
		ui->toolButton_optional3, ui->toolButton_optional4,
		ui->toolButton_optional5, ui->toolButton_optional6, 
		ui->toolButton_optional7, ui->toolButton_optional8, 
		NULL
	};

	if ( !isFfmpegAvailable( ffmpeg ) )
		return;

	//emit information( QString::fromUtf8( "2013年7月29日対応版です。" ) );
	//emit information( QString::fromUtf8( "ニュースで英会話とABCニュースシャワーは未対応です。" ) );
	//emit information( QString::fromUtf8( "----------------------------------------" ) );


       for ( int i = 0; checkbox[i] && !isCanceled; i++ ) {
       
		optional1 = MainWindow::optional1;
		optional2 = MainWindow::optional2;
		optional3 = MainWindow::optional3;
		optional4 = MainWindow::optional4;
		optional5 = MainWindow::optional5;
		optional6 = MainWindow::optional6;
		optional7 = MainWindow::optional7;
		optional8 = MainWindow::optional8;
		if ( paths[i].right( 9 ).startsWith("optional1") ) json_paths[i] = optional1;
		if ( paths[i].right( 9 ).startsWith("optional2") ) json_paths[i] = optional2;
		if ( paths[i].right( 9 ).startsWith("optional3") ) json_paths[i] = optional3;
		if ( paths[i].right( 9 ).startsWith("optional4") ) json_paths[i] = optional4;
		if ( paths[i].right( 9 ).startsWith("optional5") ) json_paths[i] = optional5;
		if ( paths[i].right( 9 ).startsWith("optional6") ) json_paths[i] = optional6;
		if ( paths[i].right( 9 ).startsWith("optional7") ) json_paths[i] = optional7;
		if ( paths[i].right( 9 ).startsWith("optional8") ) json_paths[i] = optional8;

		if ( checkbox[i]->isChecked()) {
		     QString Xml_koza = "NULL";
		     for ( int ii = 0; json_paths2[ii] != NULL; ii++ ) {
		     	if ( json_paths[i] == json_paths2[ii]  )  Xml_koza = paths2[ii];  
		     }
		
		   if ( (ui->checkBox_next_week2->isChecked()) || json_paths[i] == "0000" ) {
			QStringList fileList = getAttribute( prefix + Xml_koza + "/" + suffix, "@file" );
			QStringList kouzaList = getAttribute( prefix + Xml_koza + "/" + suffix, "@kouza" );
			QStringList hdateList = one2two( getAttribute( prefix + Xml_koza + "/" + suffix, "@hdate" ) );
			QStringList nendoList = getAttribute( prefix + Xml_koza + "/" + suffix, "@nendo" );
			QStringList dirList = getAttribute( prefix + Xml_koza + "/" + suffix, "@dir" );

			if ( fileList.count() && fileList.count() == kouzaList.count() && fileList.count() == hdateList.count() && ( ui->checkBox_next_week2->isChecked() || json_paths[i] == "0000") ) {
			     if ( Xml_koza == "NULL" && !(ui->checkBox_next_week2->isChecked()) )	continue;
				if ( true /*ui->checkBox_this_week->isChecked()*/ ) {
					for ( int j = 0; j < fileList.count() && !isCanceled; j++ ){
						QString RR = "R";
						if (json_paths[i] == "0000" )  RR = "G";
						captureStream( kouzaList[j], hdateList[j], fileList[j], nendoList[j], dirList[j], RR );
					}
				}
			}
		   }

		   if ( (!(ui->checkBox_next_week2->isChecked()) && json_paths[i] != "0000" ) || Xml_koza == "NULL" ) {
		   	QStringList fileList2 = getJsonData( json_paths[i], "file_name" );
			QStringList kouzaList2 = getJsonData( json_paths[i], "program_name" );
			QStringList file_titleList = getJsonData( json_paths[i], "file_title" );
			QStringList hdateList2 = one2two( getJsonData( json_paths[i], "onair_date" ));
			QStringList yearList = getJsonData( json_paths[i], "open_time" );
			
			if ( fileList2.count() && fileList2.count() == kouzaList2.count() && fileList2.count() == hdateList2.count() ) {
					for ( int j = 0; j < fileList2.count() && !isCanceled; j++ ){
						if ( fileList2[j] == "" || fileList2[j] == "null" ) continue;
						captureStream_json( kouzaList2[j], hdateList2[j], fileList2[j], yearList[j], file_titleList[j] );
					}
			}
		   }}		   
	  }
	
	//if ( !isCanceled && ui->checkBox_shower->isChecked() )
		//downloadShower();

	//if ( !isCanceled && ui->checkBox_14->isChecked() )
		//downloadENews( false );

	//if ( !isCanceled && ui->checkBox_15->isChecked() )
		//downloadENews( true );

	emit current( "" );
	//キャンセル時にはdisconnectされているのでemitしても何も起こらない
	emit information( QString::fromUtf8( "レコーディング作業が終了しました。" ) );
}
