#ifndef _MRC_NETWORK_HTTP_H_
#define _MRC_NETWORK_HTTP_H_

#include "mrc_base.h"
#include "mrc_network.h"


/*
以下是基于HTTP协议的网络接口，
由于HTTP是基于socket上的数据包格式重定义，所以熟悉HTTP协议及socket开发
的朋友，可以自己开发定义，该部分的源码可以到freesky.51mrp.com上下载
*/


/**
 * \brief max http header size in bytes
 */
#ifndef HTTP_HEADER_SIZE
#define HTTP_HEADER_SIZE				1024
#endif

/**
 * \brief max field value length in response header
 */
#ifndef HTTP_FIELDVALUE_SIZE
#define HTTP_FIELDVALUE_SIZE			128
#endif



/**
 * \brief supported http method
 */
typedef enum HTTP_METHOD_E
{
	GET = 0, //"GET"
	
	HEAD, //"HEAD"

	POST//"POST"
}HTTPMETHOD;

/**
 * \brief http events
 */
typedef enum HTTP_EVENT_E
{
	/** sent when http socket connected */
	HTTPEVT_CONNECTED = SOCKEVT_CONNECTED,

	/** sent when http socket connect failed */
	HTTPEVT_CONNECTFAILED = SOCKEVT_CONNECTFAILED,

	/** sent when error happen */
	HTTPEVT_ERROR = SOCKEVT_ERROR,

	/** sent when socket closed */
	HTTPEVT_CLOSED = SOCKEVT_CLOSED,

	/** sent when get the http response header */
	HTTPEVT_RESPONSE_HEADER,

	/** sent when get the http response data */
	HTTPEVT_RESPONSE_DATA,

	/** sent when one http response finished */
	HTTPEVT_RESPONSE_END
}HTTPEVENT;


/**
 * 函数名称:mrc_Http_Initialize
 * 函数功能: HTTP初始化
 * 返回值:无
 */
VOID mrc_Http_Initialize(VOID);

/**
 * \brief Terminate the http module.
 *
 * When the http module is not needed by the application anymore, this function could
 * be called to release the http module.
 *
 * \sa Socket_Initialize
 */
VOID mrc_Http_Terminate(VOID);

/**
 * \brief Open a socket for http.
 *
 * \param ip_string the server ip address or host name,just like 192.168.0.1 or http://www.163.com
 * only the authorization user can use this function
 * \param port the server port number
 * \param fnEvent the http events handler
 * \return http socket handle on success, NULL otherwise
 */
PSOCKET mrc_Http_OpenEx(char*ip_string,uint16 port,FN_SOCKEVENT fnEvent); 


/**
 * \brief Close a http socket .
 * 
 * \param socket the socket handle
 * \param evt the close event
 * \return TRUE on success, FALSE otherwise
 */
BOOL mrc_Http_Close(PSOCKET socket, DWORD evt);



/**
 * \brief Send http get command.
 *
 * \param socket the http socket handle
 * \param url the request url
 * \param from the start position of the content
 * \param to the end position of the content
 * \return TRUE on success, FALSE otherwise
 */
 BOOL mrc_Http_GetEx(PSOCKET socket, PCSTR url, uint32 from, uint32 to);



/**
 * \brief Post the http data.
 *
 * \param socket the http socket
 * \param url the request url
 * \param buffer the post data
 * \param size post data size
 * \return TRUE on success, FALSE otherwise
 */
BOOL mrc_Http_PostEx(PSOCKET socket, PCSTR url, PCSTR buffer, uint32 size);
/**
 * \brief Get the http response code.
 *
 * This can only be called after receive the EVENT HTTPEVT_RESPONSE_HEADER event.
 * otherwise undefined.
 *
 * \param socket the http socket handle
 * \return the response code
 */
uint32 mrc_Http_GetResponseCode(PSOCKET socket);

/**
 * \brief Get the field value of the http header
 *
 * This can only be called after receive the EVENT HTTPEVT_RESPONSE_HEADER event.
 * otherwise undefined.
 *
 * \param socket the http socket handle
 * \param field the http header field
 * \return the value of the filed on success, NULL otherwise
 */
PCSTR mrc_Http_GetResponseField(PSOCKET socket, PCSTR field);


/**
* \brief Get http response head content
*
*\param socket the http socket handle
* \param buffer the http head point
*\param size the http head data length
 * \return the value of MR_SUCCESS on success, MR_FAILED otherwise
*/
int mrc_Http_GetResponseHead(PSOCKET socket,uint8** buf, uint32* size);




#endif
