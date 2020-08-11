/*

*/

#ifndef XunfeiOCR_h
#define XunfeiOCR_h

#include <inttypes.h>
#include "Stream.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "MD5.h"
#include <rBase64.h>


#define		PIC_PACK_SZ             600
#define		HTTP_POST_TIME_OUT     	10000
#define		POST_PACKET_DELAY       5 
class XunfeiOCR
{
  private:
	String 		APIKEY; 
	String		APPID;
	time_t  	getNowsecs();
	void 			printtime();
	String 		CalXunfeiMD5(time_t nowSecs);	
	uint32_t  CalBodySz(char *buf,uint32_t sz);
	String 		urlEncode(char *source,uint32_t sz) ;
	int32_t 	PostBody(WiFiClient *client,char *buf,uint32_t sz);
	int32_t 	GetJson(String text,String *json);
  public:
	XunfeiOCR(String APPID,String APIKEY);
	void 			NTP();
	int32_t 	GetText(char *pic,uint32_t sz,String *json);
};


#endif

