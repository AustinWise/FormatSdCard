// FormatSdCard.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

class HandleHolder
{
private:
	HANDLE mHand;

public:
	//HandleHolder()
	//	:mHand(INVALID_HANDLE_VALUE)
	//{
	//}

	HandleHolder(HANDLE h)
		:mHand(h)
	{
	}

	HandleHolder(const HandleHolder& other) = delete;
	HandleHolder(HandleHolder&& other) = delete;

	bool isValid()
	{
		return mHand != INVALID_HANDLE_VALUE;
	}

	HANDLE value()
	{
		assert(mHand != INVALID_HANDLE_VALUE);
		return mHand;
	}

	~HandleHolder()
	{
		if (mHand != INVALID_HANDLE_VALUE)
		{
			CloseHandle(mHand);
			mHand = INVALID_HANDLE_VALUE;
		}
	}
};

static void PrintLastErrorAndDie()
{
	DWORD lastError = GetLastError();
	TCHAR *str;
	DWORD rc = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		lastError,
		0,
		reinterpret_cast<LPWSTR>(&str),
		0,
		NULL);
	if (rc == 0)
	{
		DWORD fmtMsgLastErr = GetLastError();
		printf("error code: %u\nFormatMessage also failed: %u\n", lastError, fmtMsgLastErr);
		abort();
	}
	_tprintf(_T("%s\n"), str);
	LocalFree(str);
	abort();
}

template <class type>
void TryDevIoControlGet(HandleHolder& hDevice, _In_ DWORD dwIoControlCode, type& outBuf)
{
	DWORD bytesReturn = 0;
	BOOL worked = false;
	size_t expectedSize = sizeof(type);

	worked = DeviceIoControl(hDevice.value(), dwIoControlCode, NULL, 0, reinterpret_cast<void*>(&outBuf), expectedSize, &bytesReturn, NULL);
	if (!worked)
	{
		PrintLastErrorAndDie();
	}
	assert(worked);
	assert(bytesReturn <= expectedSize);
}

static DWORD ConvertBytesToGB(LARGE_INTEGER bytes)
{
	return (DWORD)(bytes.QuadPart / 1024LL / 1024LL / 1024LL);
}

static DWORD ConvertBytesToMB(LARGE_INTEGER bytes)
{
	return (DWORD)(bytes.QuadPart / 1024LL / 1024LL);
}

static void printPartitions(HandleHolder& hDrive)
{
	BYTE bigBuffer[10240];
	TryDevIoControlGet(hDrive, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, bigBuffer);
	//TODO: look up for the rules for aliasing to see if this is safe
	DRIVE_LAYOUT_INFORMATION_EX *layout = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX *>(&bigBuffer);

	for (size_t i = 0; i < layout->PartitionCount; i++)
	{
		PARTITION_INFORMATION_EX* part = &layout->PartitionEntry[i];
		if (part->PartitionStyle == PARTITION_STYLE_MBR)
		{
			if (part->Mbr.PartitionType == PARTITION_ENTRY_UNUSED)
				continue;
			printf("\tMBR %u: start: %u length: %u\n", part->PartitionNumber, ConvertBytesToMB(part->StartingOffset), ConvertBytesToMB(part->PartitionLength));
		}
		else if (part->PartitionStyle == PARTITION_STYLE_GPT)
		{
			printf("\tGPT %u: start: %u length: %u\n", part->PartitionNumber, ConvertBytesToMB(part->StartingOffset), ConvertBytesToMB(part->PartitionLength));
		}
	}
}

int main()
{
	TCHAR buf[1024];
	const int bufSize = sizeof(buf) / sizeof(buf[0]) - 1;

	//IOCTL_DISK_CREATE_DISK
	//IOCTL_DISK_GET_DRIVE_GEOMETRY
	//IOCTL_DISK_UPDATE_PROPERTIES
	//IOCTL_DISK_SET_DRIVE_LAYOUT_EX 
	//IOCTL_DISK_GET_DRIVE_LAYOUT_EX
	//IOCTL_DISK_IS_WRITABLE
	//IOCTL_MOUNTDEV_QUERY_DEVICE_NAME

	for (int i = 0; i < 64; i++)
	{
		int driveNameLength = _sntprintf_s(buf, bufSize, _T("\\\\.\\PhysicalDrive%d"), i);
		assert(driveNameLength > 0 && driveNameLength < sizeof(buf));
		HandleHolder hDrive(CreateFile(buf, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL));
		if (!hDrive.isValid())
		{
			DWORD lastError = GetLastError();
			assert(lastError == ERROR_FILE_NOT_FOUND);
		}
		else
		{
			DISK_GEOMETRY_EX driveGeomEx;
			TryDevIoControlGet(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, driveGeomEx);

			PARTITION_INFORMATION_EX partInfo;
			TryDevIoControlGet(hDrive, IOCTL_DISK_GET_PARTITION_INFO_EX, partInfo);

			DWORD sizeInGb = ConvertBytesToGB(driveGeomEx.DiskSize);
			bool isRemovable = driveGeomEx.Geometry.MediaType == MEDIA_TYPE::RemovableMedia;
			_tprintf(_T("%s: %s disk size: %u GB\n"), buf, isRemovable ? _T("removable") : _T("fixed"), sizeInGb);

			//printPartitions(hDrive);
			if (isRemovable)
			{
				//DWORD bytesReturned;
				//int rc = DeviceIoControl(hDrive.value(), IOCTL_DISK_DELETE_DRIVE_LAYOUT, NULL, 0, NULL, 0, &bytesReturned, NULL);
				//if (rc == 0)
				//{
				//	PrintLastErrorAndDie();
				//}
			}
		}
	}

	return 0;
}

