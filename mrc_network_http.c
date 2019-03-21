#include "mrc_base.h"
#include "mrc_network.h"
#include "mrc_network_http.h"
#include "stdarg.h"//for va_list

typedef DWORD HBLOCKHEAP;

#ifndef MAX_HOSTNAME_LEN
#define MAX_HOSTNAME_LEN		112
#endif
#define HTTP_RESPONSECODE_OFFSET		9

static const char sHttpMethods[][5] = 
{
	"GET",			
	"HEAD",
	"POST"
};

typedef struct HttpRequest_T
{
	HTTPMETHOD method;
	struct HttpRequest_T* next;
}HTTPREQUEST, *PHTTPREQUEST;

typedef struct HttpData
{
	//http event handler
	FN_SOCKEVENT fnEvent;

	//available when header responsed
	uint32 responsecode;
	int32 contentLength;

	//cache the http header
	uint32 hdrsize;
	char header[HTTP_HEADER_SIZE];
	//http request list on this connection
	PHTTPREQUEST requests_head; 
	PHTTPREQUEST requests_tail;
}HTTPDATA, *PHTTPDATA;

static HTTPDATA sHttpDataPool[SOCKET_MAX_COUNT];
static HBLOCKHEAP sHttpDataHeap;


HBLOCKHEAP BlockHeap_Initialize(VOID* buffer, int buffersize, int blocksize);
VOID BlockHeap_Free(HBLOCKHEAP* pHeap, VOID* pBlock);
VOID* BlockHeap_Alloc(HBLOCKHEAP* pHeap);

static int mrc_i_isNum(uint8 code);
static int mrc_i_isNumText(char * code);
static uint32 inet_addr(const char* cp);
static char* tolower(char* str);
static PCSTR host(PCSTR url, uint16* port, uint16 def);
static uint16 get_port(PCSTR url);

static char* tolower(char* str)
{
	int i = 0;

	for(; str[i]; i++)
	{
		if(str[i] >= 'A' && str[i] <= 'Z') 
			str[i] += 'a' - 'A';
	}

	return str;
}


static PCSTR host(PCSTR url, uint16* port, uint16 def)
{
	static char host[MAX_HOSTNAME_LEN];
	int i, j;
	if(port) *port = def;
	
	for(i = j = 0; url[i]; i++)
	{
		if(url[i] == ':' && url[i+1] == '/' && url[i+2] == '/')
		{
			i+=3;
			while(url[i] && url[i]!=':' && url[i]!='/') 
			{
				host[j++] = url[i++];
			}
			
			if(port && url[i] == ':') 
			{
				*port = atoi(url+i+1);
			}
			break;
		}
	}
	host[j] = 0;
	return host;	
}


static uint16 get_port(PCSTR url)
{
	uint16 port = 0;
	char* p = NULL;
	
	if( NULL == url )
	{
		return 80;
	}
	
	p = mrc_strstr(url,"://");
	if( p )
	{
		p += 3;
	}
	else
	{
		p = (char*)url;
	}

	p = mrc_strstr(p,":");
	if( p )
	{
		port = atoi(p+1);
	}
	else
	{
		port = 80;
	}

	if( port  == 0 )
	{
		port = 80;
	}
	return port;
	
}
static int mrc_i_isNum(uint8 code)
{
	if ((code>47 && code<58) || (code=='.') || (code==':') )
	{
		return 1;
	}
	return 0;
}
static int mrc_i_isNumText(char * code)
{
	char * tempCode = code;

	while(*tempCode)
	{
		if( mrc_i_isNum(*tempCode) ==0 )
		{
			return 0;
		}
		tempCode++; 
	}
	return 1;
}
static uint32 inet_addr(const char* cp)
{
	uint32 ip = 0;
	int i;
	char  ipstr[100];
	mrc_memset(ipstr, 0, 100);

	for(i = 0; i < 4; i++)
	{
		ip <<= 8;
		ip |= atoi(cp);
		
		if(i < 3 && (NULL == (cp = mrc_strchr(cp, '.'))))
			return 0;
		cp++;
	}
	return ip;
}

#ifdef HTTP_HEAD_METHOD_ENABLE	
static BOOL mrc_Http_Request_Add(PHTTPDATA pHttpData, HTTPMETHOD method)
{
	PHTTPREQUEST request;

	//append to the request list, must not failed
	if( NULL == (request = (PHTTPREQUEST)malloc(sizeof(HTTPREQUEST))))
	{
		
		return FALSE;
	}

	//add to the request list
	request->method = method;
	request->next = NULL;
	if(pHttpData->requests_head == NULL)
		pHttpData->requests_head = request;
	else
		pHttpData->requests_tail->next = request;
	pHttpData->requests_tail = request;
	
	return TRUE;
}


static void mrc_Http_Request_Clear(PHTTPDATA pHttpData)
{
	PHTTPREQUEST request;

	while(pHttpData->requests_head)
	{
		request = pHttpData->requests_head;
		pHttpData->requests_head = request->next;
		free(request);
	}
}


static void mrc_Http_Request_Pop(PHTTPDATA pHttpData)
{
	PHTTPREQUEST request = pHttpData->requests_head;
	
	if(request)
	{
		pHttpData->requests_head = request->next;
		if(pHttpData->requests_head == NULL)
			pHttpData->requests_tail = NULL;
		
		free(request);
	}
}
#endif


#define FIND_HTTP_HEADER_ENDING(pHttpData) \
	(pHttpData->hdrsize >= 4 \
	&& pHttpData->header[pHttpData->hdrsize - 4] == '\r' \
	&& pHttpData->header[pHttpData->hdrsize - 3] == '\n' \
	&& pHttpData->header[pHttpData->hdrsize - 2] == '\r' \
	&& pHttpData->header[pHttpData->hdrsize -1] == '\n')


#define RESET_HTTP_RESPONSE_INFO(pHttpData) \
	pHttpData->contentLength = 0; \
	pHttpData->hdrsize = 0; \
	pHttpData->responsecode = 0


#ifdef HTTP_HEAD_METHOD_ENABLE
#define HANDLE_RESPONSE_END(socket, pHttpData) \
	if(pHttpData->fnEvent(socket, HTTPEVT_RESPONSE_END, NULL)) \
		return 1; \
	RESET_HTTP_RESPONSE_INFO(pHttpData); \
	Http_Request_Pop(pHttpData)
#else
#define HANDLE_RESPONSE_END(socket, pHttpData) \
	if(pHttpData->fnEvent(socket, HTTPEVT_RESPONSE_END, NULL)) \
		return 1; \
	RESET_HTTP_RESPONSE_INFO(pHttpData)
#endif


static DWORD mrc_Http_HandleResponseData(PSOCKET socket, PSOCKEVTDATA data)
{
	int32 i;
	PCSTR len;
	PHTTPDATA pHttpData = (PHTTPDATA)socket->userdata;

	if(pHttpData->responsecode == 0)
	{
		//receive the http header
		for(i = 0; i < data->size; i++)
		{
			//check the http header buffer, when this happen user should adjust the value of HTTP_HEADER_SIZE
			//todo
			if(pHttpData->hdrsize >= HTTP_HEADER_SIZE)
			{
				mrc_Http_Close(socket, HTTPEVT_ERROR);
				return 1;
			}

			//copy the byte and continue when not found the http header ending
			pHttpData->header[pHttpData->hdrsize++] = data->buffer[i];
			if(!FIND_HTTP_HEADER_ENDING(pHttpData))
				continue;

			//find the header
			pHttpData->header[pHttpData->hdrsize] = 0;
			//SGL_LOG("header.txt", pHttpData->header, pHttpData->hdrsize);
			tolower(pHttpData->header);

			//maybe socket been closed, user do not want to deal the data left
			//Note:changed by huangsunbo 2008-10-20,respond head data
			//todo
			if(pHttpData->fnEvent(socket, HTTPEVT_RESPONSE_HEADER, data)) 
				return 1;
			
#ifdef HTTP_HEAD_METHOD_ENABLE	
			if(pHttpData->requests_head->method == HEAD)
			{
				HANDLE_RESPONSE_END(socket, pHttpData);
			}else{
#endif				
				if(NULL == (len = mrc_Http_GetResponseField(socket, HTTP_FIELD_CONTENTLENGTH))	)
				{
					//can not support the response do not have the field "Content-Length"
					mrc_Http_Close(socket, HTTPEVT_ERROR);
					return 1;
				}
				
				pHttpData->contentLength = atoi(len);
				pHttpData->responsecode = mrc_Http_GetResponseCode(socket);
				if(pHttpData->contentLength == 0)
				{
					HANDLE_RESPONSE_END(socket, pHttpData);
				}
#ifdef HTTP_HEAD_METHOD_ENABLE	
			}
#endif

			i++; //i is the bytes has been copyed, so should add 1
			break;
		}

		data->buffer += i;
		data->size -= i;
	}

	if(data->size > 0)
	{
		int32 notifybytes = MIN(pHttpData->contentLength, data->size);
		int32 remainbytes = data->size - pHttpData->contentLength;	
		data->size = notifybytes;

		//maybe socket been closed, user do not want to deal the data left
		if(pHttpData->fnEvent(socket, HTTPEVT_RESPONSE_DATA, data))
			return 1;

		pHttpData->contentLength -= notifybytes;
		data->size -= notifybytes;
		data->buffer += notifybytes;
		data->size = remainbytes;

		if(pHttpData->contentLength == 0)
		{
			HANDLE_RESPONSE_END(socket, pHttpData);
		}
		
		if(data->size > 0)
			return mrc_Http_HandleResponseData(socket, data);
	}

	return 0;
}


static void mrc_Http_Destroy(PSOCKET socket)
{
	PHTTPDATA pHttpData = (PHTTPDATA)socket->userdata;

#ifdef HTTP_HEAD_METHOD_ENABLE		
	Http_Request_Clear(pHttpData);
#endif
	BlockHeap_Free(&sHttpDataHeap, pHttpData);
}


static DWORD mrc_Http_HandleSocketEvents(PSOCKET socket, DWORD evt, PSOCKEVTDATA data)
{
	PHTTPDATA pHttpData = (PHTTPDATA)socket->userdata;
	FN_SOCKEVENT fnEvent  = pHttpData->fnEvent;

	switch(evt)
	{
	
	case SOCKEVT_CONNECTED:
	{
		return fnEvent(socket, evt, data);
	}
	
	case SOCKEVT_RECVDATA:
	{	
		/* 
		* HTTPEVT_RESPONSE_HEADER sent to the user after get the http response header
		* HTTPEVT_RESPONSE_DATA sent to the user when receive the http data, 
		* HTTPEVT_RESPONSE_END sent to the user when request finished
		*/
		return mrc_Http_HandleResponseData(socket, data);
	}

	case SOCKEVT_CONNECTFAILED:
	case SOCKEVT_ERROR:
	case SOCKEVT_CLOSED:
	{
		mrc_Http_Destroy(socket);
		return fnEvent(socket, evt, data);
	}
	
	}

	return 0;
}


void mrc_Http_Initialize(void)
{
	sHttpDataHeap = BlockHeap_Initialize(sHttpDataPool, sizeof(sHttpDataPool), sizeof(HTTPDATA));
}


void mrc_Http_Terminate(void)
{
	//DO NOTHING
}


/*
支持直接连接服务器方式
伍文杰 2010-03-30
*/
PSOCKET mrc_Http_OpenEx(char*ip_string,uint16 port,FN_SOCKEVENT fnEvent)
{
	PHTTPDATA pHttpData;
	PSOCKET socket;
	
	if(!ip_string)
	{
		return NULL;
	}
	
	if( NULL == (pHttpData = BlockHeap_Alloc(&sHttpDataHeap)))
	{
		return NULL;
	}

	pHttpData->fnEvent = fnEvent;
	pHttpData->contentLength = 0;
	pHttpData->responsecode = 0;
	pHttpData->hdrsize = 0;
	
#ifdef HTTP_HEAD_METHOD_ENABLE		
	pHttpData->requests_head = NULL;
	pHttpData->requests_tail = NULL;
#endif

	if(NULL == (socket = mrc_Socket_Create(SOCKPROTO_TCP, 
				mrc_Http_HandleSocketEvents, (DWORD)pHttpData)))
	{
		BlockHeap_Free(&sHttpDataHeap, pHttpData);
		return NULL;
	}
	
	if(mrc_Socket_ConnectNoProxy(socket, ip_string,port,1))
	{
		return socket;
	}
	else
	{
		mrc_Socket_Close(socket,SOCKEVT_CLOSED);
		BlockHeap_Free(&sHttpDataHeap, pHttpData); 
		return NULL;
	}
}


BOOL mrc_Http_Close(PSOCKET socket, DWORD evt)
{
	return mrc_Socket_Close(socket, evt);
}


PCSTR mrc_Http_FormatHeader(uint32* size, HTTPMETHOD method, PCSTR url,...)
{
	static char header[HTTP_HEADER_SIZE];
	uint32 hdrsize;
	PCSTR field, value;
	va_list args;
	va_start(args, url);

	//format the request header line
	sprintf(header, HTTP_HEADER_FORMAT, sHttpMethods[method], url);
	hdrsize = strlen(header);

	//parse other fields and values
	while(NULL != (field = va_arg(args, PCSTR)))
	{
		//field name
		strcpy(header + hdrsize, field);
		hdrsize += strlen(field);
		header[hdrsize++] = ':';
		header[hdrsize++] = ' ';//add space by huangsunbo

		//field value
		value = va_arg(args, PCSTR);
		strcpy(header + hdrsize, value);
		hdrsize += strlen(value);

		//field ending
		header[hdrsize++] = '\r';
		header[hdrsize++] = '\n';
	}

	//append the http header ending
	header[hdrsize++] = '\r';
	header[hdrsize++] = '\n';
	
	va_end(args);
	*size = hdrsize;
	return header;
}


BOOL mrc_Http_Send(PSOCKET socket, HTTPMETHOD method, PBYTE buffer, uint32 size)
{
#ifdef HTTP_HEAD_METHOD_ENABLE	
	PHTTPDATA pHttpData = (PHTTPDATA)socket->userdata;
#endif

	
	if(!mrc_Socket_Send(socket, buffer, size))
		return FALSE;

#ifdef HTTP_HEAD_METHOD_ENABLE	

	//append to the request list, must not failed
	if(!mrc_Http_Request_Add(pHttpData, method))
	{
		mrc_Http_Close(socket, HTTPEVT_ERROR);
		return FALSE;
	}
	
#endif

	return TRUE;
}

BOOL mrc_Http_GetEx(PSOCKET socket, PCSTR url, uint32 from, uint32 to)
{
	PCSTR pHeader, pHost;
	uint32 hdrsize;
	uint16 port;
	uint8 host_buf[128] = {0};
	
	
	pHost = host(url,NULL,80);
	port = get_port(url);
	
	//网关转发的时候需要主机和端口
	/*测试注掉*/
	sprintf((char*)host_buf,"%s:%d",pHost,port);
	
	
	//<1>CP发起的http请求，只需要通过sky的proxy服务器进行转发
	//因此只需要连接sky的server，然后把请求进行转发
	//不需要修改url


	//<2>当是cmwap拨号方式的时候,由于需要通过cmcc的网关进行转发
	//因此需要通过Host中指明的主机和端口来让网关进行转发
	
	if(from > 0)
	{
		char tmp[24] = {0};
		if(to > from)
			sprintf(tmp, "bytes=%d-%d", from, to);
		else
			sprintf(tmp, "bytes=%d-", from);

		pHeader = mrc_Http_FormatHeader(&hdrsize, GET, url, 
			HTTP_FIELD_HOST, host_buf,
			HTTP_FIELD_UA, HTTP_VALUE_UA,
			HTTP_FIELD_ACCEPT, HTTP_VALUE_ACCEPT,
			HTTP_FIELD_CONTENTTYPE, HTTP_VALUE_CONTENTTYPE,
			HTTP_FIELD_CONNECTION, HTTP_VALUE_CONNECTION_KEEPALIVE,
			HTTP_FIELD_PROXYCONNECTION, HTTP_VALUE_CONNECTION_KEEPALIVE,
			HTTP_FIELD_RANGE, tmp,
			NULL); //do not forget the NULL ending
	}
	else
	{
		/**/
		pHeader = mrc_Http_FormatHeader(&hdrsize, GET, url, 
			HTTP_FIELD_HOST,  host_buf,
			HTTP_FIELD_UA, HTTP_VALUE_UA,
			HTTP_FIELD_ACCEPT, HTTP_VALUE_ACCEPT,
			HTTP_FIELD_CONTENTTYPE, HTTP_VALUE_CONTENTTYPE,
			HTTP_FIELD_CONNECTION, HTTP_VALUE_CONNECTION_KEEPALIVE,
			HTTP_FIELD_PROXYCONNECTION, HTTP_VALUE_CONNECTION_KEEPALIVE,
			NULL); //do not forget the NULL ending
			
	}

	return mrc_Http_Send(socket, GET, (PBYTE)pHeader, hdrsize);
}


#ifdef HTTP_HEAD_METHOD_ENABLE	

BOOL mrc_Http_Head(PSOCKET socket, PCSTR url)
{
	PCSTR pHeader, pHost;
	uint32 hdrsize;
	
	pHost = host(url, NULL, 0);
	pHeader = mrc_Http_FormatHeader(&hdrsize, HEAD, url, 
		HTTP_FIELD_HOST, pHost,
		HTTP_FIELD_UA, HTTP_VALUE_UA,
		HTTP_FIELD_ACCEPT, HTTP_VALUE_ACCEPT,
		HTTP_FIELD_CONTENTTYPE, HTTP_VALUE_CONTENTTYPE,
		HTTP_FIELD_CONNECTION, HTTP_VALUE_CONNECTION_KEEPALIVE,
		HTTP_FIELD_PROXYCONNECTION, HTTP_VALUE_CONNECTION_KEEPALIVE,
		NULL); //do not forget the NULL ending

	return mrc_Http_Send(socket, HEAD, (PBYTE)pHeader, hdrsize);
}

#endif
BOOL mrc_Http_PostEx(PSOCKET socket, PCSTR url, PCSTR buffer, uint32 size)
{
	PCSTR pHeader, pHost;
	uint32 hdrsize;
	char tmp[12]={0};
	uint8 host_buf[128] = {0};
	uint16 port;


	pHost = host(url,NULL,80);
	port = get_port(url);
	
	//网关转发的时候需要主机和端口
	sprintf((char*)host_buf,"%s:%d",pHost,port);
	
	sprintf(tmp, "%d", size);
	pHeader = mrc_Http_FormatHeader(&hdrsize, POST, url, 
		HTTP_FIELD_HOST, host_buf,
		HTTP_FIELD_UA, HTTP_VALUE_UA,
		HTTP_FIELD_ACCEPT, HTTP_VALUE_ACCEPT,
		HTTP_FIELD_CONTENTTYPE, HTTP_VALUE_CONTENTTYPE,
		HTTP_FIELD_CONNECTION, HTTP_VALUE_CONNECTION_KEEPALIVE,
		HTTP_FIELD_PROXYCONNECTION, HTTP_VALUE_CONNECTION_KEEPALIVE,
		HTTP_FIELD_CONTENTLENGTH, tmp,
		NULL); //do not forget the NULL ending

	//check that socket have enough buffer
	if(socket->begin + SOCKET_SENDBUFFER_SIZE - socket->end < hdrsize + size)
	{
		return FALSE;
	}

	//if return false that cause is not about send buffer
	if(!mrc_Http_Send(socket, POST, (PBYTE)pHeader, hdrsize))
		return FALSE;

	//if return false that cause is not about send buffer
	if(size > 0 && !mrc_Socket_Send(socket, (PBYTE)buffer, size))
		return FALSE;

	return TRUE;
}


uint32 mrc_Http_GetResponseCode(PSOCKET socket)
{
	PHTTPDATA pHttpData = (PHTTPDATA) socket->userdata;
	return atoi(pHttpData->header + HTTP_RESPONSECODE_OFFSET);
}

int mrc_Http_GetResponseHead(PSOCKET socket,uint8** buf, uint32* size)
{
	PHTTPDATA pHttpData = NULL;
	if( NULL == socket  || NULL == buf || size == NULL ||
		socket->sd < 0 || socket->protocol  != SOCKPROTO_TCP )
	{
		buf = NULL;
		*size = 0;		
		return MR_FAILED;
	}

	pHttpData = (PHTTPDATA) socket->userdata;
	*buf = (uint8*)pHttpData->header;
	*size = pHttpData->hdrsize;
	return MR_SUCCESS;	
}


PCSTR mrc_Http_GetResponseField(PSOCKET socket, PCSTR field)
{
	static char value[HTTP_FIELDVALUE_SIZE];
	
	int32 i = 0;
	PCSTR pTmp;
	PHTTPDATA pHttpData = (PHTTPDATA)socket->userdata;

	strncpy(value, field, HTTP_FIELDVALUE_SIZE-1);
	value[HTTP_FIELDVALUE_SIZE-1] = 0;
	tolower(value);
	
	if( NULL == (pTmp = mrc_strstr(pHttpData->header, value)))
		return NULL;

	pTmp += mrc_strlen(field);
	while(*pTmp == ' ' || *pTmp == ':')
		pTmp++;

	while(!(pTmp[i] == '\r' && pTmp[i+1] == '\n'))
	{
		value[i] = pTmp[i];
		i++;
	}
	
	value[i] = 0; 
	return value;
}


HBLOCKHEAP BlockHeap_Initialize(VOID* buffer, int buffersize, int blocksize)
{
	PBYTE pBlock = (PBYTE)buffer;

	//SGL_ASSERT(0 == MOD(buffersize, blocksize) && 0 == MOD(blocksize, 4));

	for(buffersize -= blocksize; pBlock - (PBYTE)buffer < buffersize ; pBlock += blocksize)
		*(Uint8**)pBlock = pBlock + blocksize;

	*(PBYTE*)pBlock = NULL;
	return (HBLOCKHEAP)buffer;
}


VOID* BlockHeap_Alloc(HBLOCKHEAP* pHeap)
{
	VOID* pBlock = (VOID*)(*pHeap);

	if(pBlock)
		*pHeap =(HBLOCKHEAP)( *(PBYTE*)(*pHeap) );

	return pBlock;
}


VOID BlockHeap_Free(HBLOCKHEAP* pHeap, VOID* pBlock)
{
	*(PBYTE*)pBlock = (PBYTE)(*pHeap);
	*pHeap = (HBLOCKHEAP)pBlock;
}