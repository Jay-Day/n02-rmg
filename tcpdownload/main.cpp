#include <windows.h>
#include <cstdio>
int DownloadListToBuffer(char * buffer, int size, char * url){
	
	char urx[2000];
	strcpy(urx, url);

	printf(__FILE__ " : %i \r\n", __LINE__);
	
	
	char * urlbuf = urx;
	char * host;
	char * port;
	char addr[500]; 
	unsigned long ul = 5;
	
	sockaddr_in server;

	printf(__FILE__ " : %i \r\n", __LINE__);
	
	
	char * RequestBuffer = (char*)malloc(1024);

	printf(__FILE__ " : %i \r\n", __LINE__);
	
	host = strstr(urlbuf, "http://")+7;
	
	if(strstr(host, ":")!=NULL){
		port = strstr(urlbuf, ":")+1;
		*(port-1) = 0x00;
		port = (char*)atoi(port);
		server.sin_port = htons((int)port);
		strcpy(addr, strstr(port, "/"));
		*(strstr(port, "/"))=0x00;
	} else{
		server.sin_port = htons(80);
		strcpy(addr, strstr(host, "/"));
		*(strstr(host, "/"))=0x00;
	}

	printf(__FILE__ " : %i \r\n", __LINE__);
	
	wsprintf(RequestBuffer,"GET %s HTTP/1.0\r\nHost:%s\r\n\r\n", addr, host);
	
	
	printf(__FILE__ " : %i \r\n", __LINE__);


	//socket
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if(s==SOCKET_ERROR)
		return 0;

	printf(__FILE__ " : %i \r\n", __LINE__);
	
	ioctlsocket(s, FIONBIO, &ul);
	
	
	if(*(host)> 0x30 && *(host)<0x3A){//ip address
		server.sin_addr.s_addr = inet_addr(host);
	} else {
		server.sin_addr = *(struct in_addr*)gethostbyname(host)->h_addr_list[0];
	}

	printf(__FILE__ " : %i \r\n", __LINE__);
	server.sin_family = AF_INET;
	
	printf(__FILE__ " : %i \r\n", __LINE__);
	if(connect(s, (struct sockaddr *)&server, sizeof(server))!=0){
		printf(__FILE__ " : %i \r\n", __LINE__);
		if(WSAGetLastError()!=WSAEWOULDBLOCK) return 0;
	}

	printf(__FILE__ " : %i \r\n", __LINE__);
	
	Sleep(300);

	printf(__FILE__ " : %i \r\n", __LINE__);

	sendto(s, RequestBuffer, strlen(RequestBuffer), 0, (sockaddr*)&server, sizeof(server));

	printf(__FILE__ " : %i \r\n", __LINE__);
	
	char * bf = buffer;
	int il = size;

	printf(__FILE__ " : %i \r\n", __LINE__);
	
	DWORD t = GetTickCount();
	while((GetTickCount()-t) < 60000){
		Sleep(25);
		printf(__FILE__ " : %i \r\n", __LINE__);
		int rec = recv(s, bf, il, 0);
		if(rec==0)
			break;
		if(rec==SOCKET_ERROR) {
			printf(__FILE__ " : %i %i %i \r\n", __LINE__, WSAGetLastError(), GetLastError());
			continue;
		}
			
		printf(__FILE__ " : %i \r\n", __LINE__);
		bf += rec;
		il -= rec;
	}

	printf(__FILE__ " : %i \r\n", __LINE__);
	
	closesocket(s);
	return size-il;
}





int main(){

	WSADATA ws;
	WSAStartup(MAKEWORD(2,2),&ws);


	printf("Downloading http://kaillera.com/raw_server_list2.php?version=0.9");

	char buffer[0x8000];
	int len = DownloadListToBuffer(buffer, 0x8000, "http://kaillera.com/raw_server_list2.php?version=0.9");


	printf(buffer);

	WSACleanup();

	system("PAUSE");

	return 0;

}