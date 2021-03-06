
//#include "stdafx.h"
#include "pch.h"
#include<Windows.h>
#include<tchar.h>
#include<SetupAPI.h>
#include<iostream>
#pragma comment (lib, "Setupapi.lib")

#define SECTOR 0x200

typedef struct usb_device_info_t
{
	char	volume;
	char	friendname[256];
	int		device_num;
}usb_device_info;

void toupCase(char *&s) {
	for (int i = 0; i < strlen(s); i++)
	{
		s[i] = toupper(s[i]);
	}
}


int get_usb_device_list(usb_device_info *usb_list, int list_size)
{
	int usb_device_cnt = 0;

	char disk_path[5] = { 0 };
	char device_path[10] = { 0 };
	DWORD all_disk = GetLogicalDrives();


	int i = 0;
	DWORD bytes_returned = 0;
	STORAGE_DEVICE_NUMBER device_num;
	//printf("%d\n", all_disk);
	while (all_disk && usb_device_cnt < list_size)
	{
		if ((all_disk & 0x1) == 1)      //bitmask A,B,C
		{
			sprintf_s(disk_path, "%c:", 'A' + i);
			sprintf_s(device_path, "\\\\.\\%s", disk_path);
			printf_s("%s\n", device_path);


			if (GetDriveTypeA(disk_path) == DRIVE_REMOVABLE)   //可移动设备类型
			{
				// get this usb device id
				HANDLE hDevice = CreateFileA(device_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);



				if (DeviceIoControl(hDevice, IOCTL_STORAGE_GET_DEVICE_NUMBER,
					NULL, 0,
					&device_num, sizeof(device_num),
					&bytes_returned, (LPOVERLAPPED)NULL))
				{
					usb_list[usb_device_cnt].volume = 'A' + i;
					usb_list[usb_device_cnt].device_num = device_num.DeviceNumber;
					usb_device_cnt++;
				}
				CloseHandle(hDevice);
				hDevice = 0;
			}
		}
		all_disk = all_disk >> 1;
		i++;
	}

	return usb_device_cnt;
}

int get_usb_device_friendname(usb_device_info *usb_list, int list_size)
{
	int i = 0;
	int res = 0;
	HDEVINFO hDevInfo;
	SP_DEVINFO_DATA DeviceInfoData = { sizeof(DeviceInfoData) };

	// get device class information handle
	hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DISK, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		res = GetLastError();
		return res;
	}

	// enumerute device information
	DWORD required_size = 0;
	for (i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData); i++)
	{
		DWORD DataT;
		char friendly_name[2046] = { 0 };
		DWORD buffersize = 2046;
		DWORD req_bufsize = 2046;

		// get device friendly name
		if (!SetupDiGetDeviceRegistryPropertyA(hDevInfo, &DeviceInfoData, SPDRP_FRIENDLYNAME, &DataT, (LPBYTE)friendly_name, buffersize, &req_bufsize))
		{
			continue;
		}
		if (strstr(friendly_name, "USB") == 0)
		{
			continue;
		}

		int index = 0;
		SP_DEVICE_INTERFACE_DATA did = { sizeof(did) };
		PSP_DEVICE_INTERFACE_DETAIL_DATA pdd = NULL;

		while (1)
		{
			// get device interface data
			if (!SetupDiEnumDeviceInterfaces(hDevInfo, &DeviceInfoData, &GUID_DEVINTERFACE_DISK, index++, &did))
			{
				res = GetLastError();
				if (ERROR_NO_MORE_DEVICES == res || ERROR_NO_MORE_ITEMS == res)
					break;
			}

			// get device interface detail size
			if (!SetupDiGetDeviceInterfaceDetail(hDevInfo, &did, NULL, 0, &required_size, NULL))
			{
				res = GetLastError();
				if (ERROR_INSUFFICIENT_BUFFER == res)
				{
					pdd = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR, required_size);
					pdd->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
				}
				else
					break;
			}

			// get device interface detail
			if (!SetupDiGetDeviceInterfaceDetail(hDevInfo, &did, pdd, required_size, NULL, NULL))
			{
				res = GetLastError();
				LocalFree(pdd);
				pdd = NULL;
				break;
			}

			// get device number
			DWORD bytes_returned = 0;
			STORAGE_DEVICE_NUMBER device_num;
			HANDLE hDevice = CreateFile(pdd->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
			if (DeviceIoControl(hDevice, IOCTL_STORAGE_GET_DEVICE_NUMBER,
				NULL, 0,
				&device_num, sizeof(device_num),
				&bytes_returned, (LPOVERLAPPED)NULL))
			{
				for (int usb_index = 0; usb_index < list_size; usb_index++)
				{
					if (device_num.DeviceNumber == usb_list[usb_index].device_num)
					{
						strcpy_s(usb_list[usb_index].friendname, friendly_name);
						break;
					}
				}
			}
			CloseHandle(hDevice);
			LocalFree(pdd);
			pdd = NULL;
		}
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);
	return res;
}


DWORD readDisk(HANDLE hd, unsigned char* &out, DWORD start, DWORD size)
{
	OVERLAPPED over = { 0 };

	over.Offset = start;
	unsigned char* buffer = new unsigned char[size + 1];
	DWORD readsize;
	if (ReadFile(hd, buffer, size, &readsize, &over) == 0)
	{
		return 0;
	}

	buffer[size] = 0;
	out = buffer;
	//delete [] buffer;
	//注意这里需要自己释放内存
	return size;
}

BOOL getName(unsigned char *&name, unsigned char *src, int i) {
	if (src[i * 32] == 0) return false;
	
	int j = 0, cnt = 0;
	unsigned char tmp[50] = {0};
	if (src[i * 32 + 0xB] == 0x0F|| src[i * 32] == 0xE5)
	{
		name = tmp; return true;
	}
	
	
	//短文件名
	if (src[i*32+0x06]!=0x7E)
	{
		for (j = 0; j < 0xB; j++)
		{
			if (j == 8 && src[i * 32 + j + 1] != 0x20)  tmp[cnt++] = '.';

			if (src[i * 32 + j] != 0x20) tmp[cnt++] = src[i * 32 + j];

		}

	}
	else
	{
		for (int k = i - 1;; k--) {
			if (src[k * 32] == 0xE5)
				break;
			int m = 1;
			while (m<0x1F&&(src[k*32+m]!=0|| src[k * 32 + m] == 0&&m==0x1A))
			{
				if(src[k * 32 + m] != 0)
					tmp[cnt++] = src[k * 32 + m];
				m += 2;
				if (m == 0x0B || m == 0x0D) m = 0x0E;
			}
			if(src[k * 32 + m] == 0) 
				break;
		}

	}
	tmp[cnt] = '\0';
	printf("**查询到的信息有: %s\n", tmp);

	name = tmp;
	return true;
}


DWORD getClusterStart(HANDLE hd, char *filePath, DWORD dataStart, DWORD &fileSize, int sctPerCluster) {
	//路径分隔符
	DWORD cStart;
	unsigned char *a;
	DWORD len = readDisk(hd, a, dataStart*SECTOR, SECTOR * 5);

	const char *delim = "\\";
	char *p;
	//使用strtok函数分割路径
	p = strtok(filePath, delim);
	p = strtok(NULL, delim);
	unsigned char *tmp;
	while (p)
	{
		
		printf("\n->%s\t", p);
		int i = 0; BOOL flag = false;
		while (getName(tmp, a, i))
		{
			if (!_stricmp((const char*)tmp, p))
			{
				flag = true; break;
			}
			i++;
		}

		if (flag)
		{
			DWORD c1 = a[i * 32 + 0x14] << 16, c2 = a[i * 32 + 0x15] << 24, c3 = a[i * 32 + 0x1a], c4 = a[i * 32 + 0x1b] << 8;
			cStart = c1 + c2 + c3 + c4;
			//printf("%d ", cStart);
			DWORD s1 = a[i * 32 + 0x1F] << 24, s2 = a[i * 32 + 0x1E] << 16, s3 = a[i * 32 + 0x1D] << 8, s4 = a[i * 32 + 0x1c];
			fileSize = s1 + s2 + s3 + s4;
			if (strstr(p, ".") != NULL)
				return cStart;

			else
			{
				DWORD dirClt = dataStart + (cStart - 2)*sctPerCluster;
				//printf("%x\n\n\n", dirClt*SECTOR);
				readDisk(hd, a, dirClt*SECTOR, SECTOR * 5);
			}
		}

		p = strtok(NULL, delim);
	}

}

DWORD getClusterList(HANDLE hd, DWORD cStart, DWORD fatStart) {
	unsigned char *a;
	DWORD len = readDisk(hd, a, fatStart*SECTOR, SECTOR);
	if (len)
	{
		DWORD st = cStart * 4;
		DWORD c1 = a[st + 3] << 24, c2 = a[st + 2] << 16, c3 = a[st + 1] << 8, c4 = a[st];
		return c1 + c2 + c3 + c4;
	}
	return 0;
}

void getData(HANDLE hd, DWORD fileStart,DWORD fileSize) {
	unsigned char *a;
	DWORD len;
	int cnt = fileSize / SECTOR + 1;
	len = readDisk(hd, a, fileStart*SECTOR, SECTOR*cnt);
	
	if (len)
	{
		for (int i = 0; i < len; i++) {
			if (i % 16 == 0) printf("\n");
			if (i % 8 == 0 && i % 16) printf("\t");
			printf("%02X ", a[i]);
		}
	}

}


int _tmain(int argc, _TCHAR* argv[])
{
	bool bRes = false;

	usb_device_info usb_list[8];
	memset(usb_list, 0, 8 * sizeof(usb_device_info));
	int usb_cnt = get_usb_device_list(usb_list, 8);
	printf("System has %d USB disk.\n", usb_cnt);

	if (usb_cnt > 0)
	{
		get_usb_device_friendname(usb_list, usb_cnt);
		{
			for (int i = 0; i < usb_cnt; i++)
			{
				printf("%c: %s and %d is \n", usb_list[i].volume, usb_list[i].friendname, usb_list[i].device_num);
			}
		}
	}
	else return -1;

	//打开U盘
	HANDLE hd = CreateFile(TEXT("\\\\.\\PHYSICALDRIVE2"), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hd == INVALID_HANDLE_VALUE) return -1;

	unsigned char* a;

	//读MBR
	printf("\n***************MBR表: (offset 0)************\n");
	DWORD len = readDisk(hd, a, 0 * SECTOR, SECTOR);
	if (len)
	{
		for (int i = 0; i < len; i++) {
			if (i % 16 == 0) printf("\n");
			if (i % 8 == 0 && i % 16) printf("\t");
			printf("%02X ", a[i]);
		}

	}

	//dbr开始处
	int dbrStart = (*(a + 0x1c6)) + (*(a + 0x1c7)) << 8;
	//printf("%x\n\n", dbrStart);
	len = readDisk(hd, a, dbrStart*SECTOR, SECTOR);
	/*if (len)
	{
		for (int i = 0; i < len; i++) {
			if (i % 16 == 0) printf("\n");
			if (i % 8 == 0 && i % 16) printf("\t");
			printf("%02X ", a[i]);
		}

	}*/
	//每簇扇区数
	int sctPerCluster = (*(a + 0xD));

	//保留区大小
	int x = (*(a + 0x0f)) << 8;
	int y = (*(a + 0x0E));
	int resvSize = x + y;
	//printf("%02x %02x %x\n", *(a + 0x0f), *(a + 0x0e),resvSize);

	//fat大小
	unsigned int f1 = (*(a + 0x27)) << 24, f2 = (*(a + 0x26)) << 16, f3 = (*(a + 0x25)) << 8, f4 = (*(a + 0x24));
	long fatSize = f1 + f2 + f3 + f4;
	//printf("%02x %02x %02x %02x %x\n", (*(a + 0x27)) << 24 , (*(a + 0x26)) << 16 , (*(a + 0x25)) << 8 , (*(a + 0x24)),fatSize);

	//读取FAT表
	long fatStart = dbrStart + resvSize;

	len = readDisk(hd, a, fatStart*SECTOR, SECTOR);
	/*if (len)
	{
		for (int i = 0; i < len; i++) {
			if (i % 16 == 0) printf("\n");
			if (i % 8 == 0 && i % 16) printf("\t");
			printf("%02X ", a[i]);
		}

	}*/

	//数据区起始位置
	DWORD dataStart = dbrStart + resvSize + fatSize * 2;

	len = readDisk(hd, a, dataStart*SECTOR, SECTOR * 2);
	/*if (len)
	{
		for (int i = 0; i < len; i++) {
			if (i % 16 == 0) printf("\n");
			if (i % 8 == 0 && i % 16) printf("\t");
			printf("%02X ", a[i]);
		}

	}*/

	printf("\n\n\n\t\t\t*********U盘信息如下");
	printf("\n***********每簇扇区数为 0x%02x\n", sctPerCluster);
	printf("\n***************DBR表: (offset %x)************\n", dbrStart);
	printf("\n***************FAT表: (offset %x)************\n", fatStart*SECTOR);
	printf("\n***************DATA区: (offset %x)************\n", dataStart*SECTOR);


	//输入路径
	//E:\dir\dir1\dir23333333333333333\dir3123456789\hello555555555555555555.txt
	char *fp;
	fp = (char*)malloc(256);
	printf("\n\n\n**************请输入要查询的文件路径名(用 \\ 表示路径)************\n\n\n");
	
	while (scanf("%s", fp)) {

	toupCase(fp);
	DWORD fileSize;
	DWORD cStart= getClusterStart(hd, fp, dataStart,fileSize,sctPerCluster);
	printf("\n\n\n**********该文件簇链 0x%x", cStart);

	//循环得到簇链
	DWORD next = getClusterList(hd, cStart, fatStart);
	while (next != 0x0fffffff)
	{
		printf("->0x%x ", next);
		next = getClusterList(hd, next, fatStart);
	}

	printf("\n\n**********该文件大小为 %dB, 顺便看一下该文件内容，如下:\n",fileSize);
	getData(hd, (dataStart + (cStart - 2)*sctPerCluster),fileSize);
	printf("\n");
	}



	CloseHandle(hd);
	getchar();
	return 1;
}

