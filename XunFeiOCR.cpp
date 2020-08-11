/*
  
*/

extern "C" {
  #include <stdlib.h>
  #include <string.h>
  #include <inttypes.h>
  #include "libb64/cdecode.h"
  #include "libb64/cencode.h"
}

#include <SPI.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "XunFeiOCR.h"
#include <StreamString.h>
#include <rBase64.h>

// Constructors ////////////////////////////////////////////////////////////////
XunfeiOCR::XunfeiOCR(String APPID,String APIKEY)
{
	this->APIKEY=APIKEY;
	this->APPID=APPID;
}

void XunfeiOCR::NTP() 
{
	
  //configTime(0, 0, "cn.pool.ntp.org", "time.nist.gov");
	configTime(0, 0, "ntp1.aliyun.com", "ntp7.aliyun.com");
  Serial.print(F("Waiting for NTP time sync: "));
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    Serial.print(F("."));
    yield();
    nowSecs = time(nullptr);
  }

  Serial.println();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
}


time_t  XunfeiOCR::getNowsecs()
{
    time_t nowSecs = time(nullptr);    
    //Serial.print(F("\r\nCurrent Sec: "));
    //Serial.print(nowSecs);
    return nowSecs;
}


void XunfeiOCR::printtime()
{
  time_t nowSecs = time(nullptr);
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print(F("\r\nCurrent time: "));
  Serial.print(asctime(&timeinfo));
  Serial.print(F("\r\nCurrent Sec: "));
  Serial.print(nowSecs);
}

const char xunfei_pram[]="eyJsYW5ndWFnZSI6ICJjbnxlbiIsICJsb2NhdGlvbiI6ICJ0cnVlIn0=";
String 	XunfeiOCR::CalXunfeiMD5(time_t nowSecs)	
{
  String    checkstr,checksum;
  char      checkbuf[200];
  uint16_t  len=0;

  checkstr=this->APIKEY+String(nowSecs)+String(xunfei_pram);
  //Serial.println(checkstr);   //合并三项为字符串
  len=checkstr.length();
  //Serial.println(len);
  memset(checkbuf,0,sizeof(checkbuf));
  checkstr.toCharArray(checkbuf,len+1);
  //Serial.println(checkbuf);
  unsigned char* hash=MD5::make_hash(checkbuf);
  char *md5str = MD5::make_digest(hash, 16);
  //Serial.println(md5str);
  return String(md5str);
}

String XunfeiOCR::urlEncode(char *source,uint32_t sz) 
{
  String encodedString = "";
  for (int i = 0; i < sz; i++) 
  {
    char c = source[i];
    if (isAlphaNumeric(c)) 
    {
      encodedString += c;
    } 
    else if (c == ' ') 
    {
      encodedString += "+";
    } 
    else 
    {
      encodedString += String("%") + String(((int)c), HEX);
    }
  }
  return encodedString;
}


const uint8_t XUNFEI_OCR_JSON_BODY_START[]="image=";
uint32_t  XunfeiOCR::CalBodySz(char *buf,uint32_t sz)
{
  uint32_t  base64Packsz,i,t_sz=sizeof(XUNFEI_OCR_JSON_BODY_START)-1;
  int32_t   ret;
  char      m_Base64Buf[(PIC_PACK_SZ/3)*4+100];

  for(i=0;i<sz;i=i+PIC_PACK_SZ)
  {
    if((sz-i)>=PIC_PACK_SZ)
      base64Packsz=rbase64_encode((char *)(m_Base64Buf),(char *)(&buf[i]),PIC_PACK_SZ);
    else
      base64Packsz=rbase64_encode((char *)(m_Base64Buf),(char *)(&buf[i]),(sz-i));

    String Strdata=this->urlEncode(m_Base64Buf,base64Packsz);
    t_sz=t_sz+Strdata.length();
  }
  return t_sz;
}

int32_t XunfeiOCR::PostBody(WiFiClient *client,char *buf,uint32_t sz)
{
  uint32_t  base64Packsz,i,t_sz=sizeof(XUNFEI_OCR_JSON_BODY_START)-1;
  int32_t   ret;
  char      m_Base64Buf[(PIC_PACK_SZ/3)*4+100];
 
  if(client->write((uint8_t*)XUNFEI_OCR_JSON_BODY_START,sizeof(XUNFEI_OCR_JSON_BODY_START)-1)<0)
    return -1;
  
  //Serial.write((uint8_t *)XUNFEI_OCR_JSON_BODY_START,sizeof(XUNFEI_OCR_JSON_BODY_START)-1);
	delay(POST_PACKET_DELAY);
  for(i=0;i<sz;i=i+PIC_PACK_SZ)
  {
    if((sz-i)>=PIC_PACK_SZ)
      base64Packsz=rbase64_encode((char *)(m_Base64Buf),(char *)(&buf[i]),PIC_PACK_SZ);
    else
      base64Packsz=rbase64_encode((char *)(m_Base64Buf),(char *)(&buf[i]),(sz-i));

    String Strdata=this->urlEncode(m_Base64Buf,base64Packsz);
    if(client->print(Strdata)<0)
      return -2;
      
    t_sz=t_sz+Strdata.length();
		delay(POST_PACKET_DELAY);
    //Serial.print(Strdata);
  }
  //Serial.print("\r\nPost pic base64 sz:");
  //Serial.print(t_sz);
  return t_sz;
}

int32_t XunfeiOCR::GetJson(String text,String *json)
{
  int32_t stpt=-1,endpt=-1;
  stpt=text.indexOf('{');
  endpt=text.lastIndexOf('}');
  if((stpt>0)&&(endpt>0))
  {
    endpt=endpt+1;
    *json=text.substring(stpt,endpt);
    return  1;
  }
  return -1;
} 


#define		EN_DEBUG_XUNFEIOCR_FUN		1
int32_t XunfeiOCR::GetText(char *pic,uint32_t sz,String *json)
{
  WiFiClient client;

  time_t  nowSecs=this->getNowsecs();
  String  Checksum=this->CalXunfeiMD5(nowSecs);
  
  #if	EN_DEBUG_XUNFEIOCR_FUN==1
		//this->printtime();
		//Serial.print("\r\n Checksum:");
		//Serial.print(Checksum);
		Serial.print("\r\nStarting connection to server...");
  #endif
  
  const char*  server = "webapi.xfyun.cn"; 
  if (!client.connect(server, 80)) 
  {
     Serial.println("Connection failed_1!");
     client.stop();
     return -1;
  }
  Serial.println("\r\nConnected to server!");
  if(!client.connected())
  {
    Serial.println("Connection failed_2!");
    client.stop();
    return -2;
  }
  // Make a HTTP request:
  String  url="/v1/service/v1/ocr/general";
  String  host="webapi.xfyun.cn";
  String  header=String("POST ") + url + " HTTP/1.1\r\n" +
           "Host: " + host + "\r\n" +
           "User-Agent: ESP8266HTTPClient\r\n" + 
           "Accept-Encoding: gzip, deflat\r\n"+
            "Accept: */*\r\n"+
           "Connection: close\r\n"+
           "X-CurTime: "+String(nowSecs)+"\r\n"+
           "X-Param: eyJsYW5ndWFnZSI6ICJjbnxlbiIsICJsb2NhdGlvbiI6ICJ0cnVlIn0="+"\r\n"+
           "X-Appid: "+this->APPID+"\r\n"+
           "X-CheckSum: "+Checksum+"\r\n"+
           "Content-Type: application/x-www-form-urlencoded; charset=utf-8"+"\r\n"+
           "Content-Length: "+String(CalBodySz((char *)pic,sz))+"\r\n"+"\r\n";
           //"Content-Length: "+String(body.length())+"\r\n"+"\r\n";
	
	#if	EN_DEBUG_XUNFEIOCR_FUN==1
		//Serial.print("\nHeader:\n");
		Serial.print(header);
  #endif
	
  client.print(header);
  PostBody(&client,(char *)(pic),sz);
  unsigned long timeout;
  timeout = millis();
  while (client.available() == 0) 
  {
    if (millis() - timeout > HTTP_POST_TIME_OUT) 
    {
      Serial.print("\r\n");
      Serial.print(">>> Client Timeout !");
      client.stop();
      return -3;
    }
    delay(POST_PACKET_DELAY);
  }
  
  
  String line ="";
  while(client.available())
  {
    line = client.readStringUntil('\r');
    delay(POST_PACKET_DELAY);
  }
	#if	EN_DEBUG_XUNFEIOCR_FUN==1
		Serial.print("\r\n-------------------------------------");
		Serial.print(line);
	#endif
	
  if(GetJson(line,json)<0)
  {
		Serial.print("\r\n JSON Error");
    client.stop();
    return -4;
  }
  client.stop();
	return 1;
}











