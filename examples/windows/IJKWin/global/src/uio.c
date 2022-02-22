#include <sys/uio.h>
#include <utils.h>
#include <errno.h>

ssize_t readv(int fd,struct iovec const*  vector, int count)
{
	int             i;
	size_t          total;
	void*           pv;
	ssize_t  ret;
	HANDLE  h = NULL;
	DWORD   dw;

	for(i = 0, total = 0; i < count; ++i)
	{
		total += vector[i].iov_len;
	}

	pv = HeapAlloc(GetProcessHeap(), 0, total);

	if(NULL == pv)
	{
		errno = Errno_From_Win32(GetLastError());

		ret = -1;
	}
	else
	{
		h = (HANDLE)Win32_Handle_From_File(fd);

		if(!ReadFile(h, pv, (DWORD)total, &dw, NULL))
		{
			errno = Errno_From_Win32(GetLastError());

			ret = -1;
		}
		else
		{
			for(i = 0, ret = 0; i < count && 0 != dw; ++i)
			{
				size_t n = (dw < vector[i].iov_len) ? dw : vector[i].iov_len;

				(void)memcpy(vector[i].iov_base, (char const*)pv + ret, n);

				ret +=  (ssize_t)n;
				dw  -=  (DWORD)n;
			}
		}

		(void)HeapFree(GetProcessHeap(), 0, pv);
	}

	return ret;
}

ssize_t writev(int fd, struct iovec const*  vector,int count)
{
	int             i;
	size_t          total;
	void*           pv;
	ssize_t  ret;
	HANDLE  h = NULL;
	DWORD   dw;

	/* Determine the total size. */
	for(i = 0, total = 0; i < count; ++i)
	{
		total += vector[i].iov_len;
	}

	pv = HeapAlloc(GetProcessHeap(), 0, total);

	if(NULL == pv)
	{
		errno = Errno_From_Win32(GetLastError());

		ret = -1;
	}
	else
	{
		h = (HANDLE)Win32_Handle_From_File(fd);

		for(i = 0, ret = 0; i < count; ++i)
		{
			(void)memcpy((char*)pv + ret, vector[i].iov_base, vector[i].iov_len);

			ret += (ssize_t)vector[i].iov_len;
		}

		if(!WriteFile(h, pv, (DWORD)total, &dw, NULL))
		{
			errno = Errno_From_Win32(GetLastError());

			ret = -1;
		}
		else
		{
			ret = (int)dw;
		}

		(void)HeapFree(GetProcessHeap(), 0, pv);
	}

	return ret;
}
