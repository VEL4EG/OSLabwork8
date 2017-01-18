#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#define STR_SIZE 100
#define READERS_COUNT 10
#define LINES_COUNT 10

struct readerArgs
{
	int fd;
	size_t number;
};

typedef struct readerArgs readerArgs;

ssize_t pgetLine(int fd, char *str, size_t maxLength, off_t offset);
void getCurrentTime(char *str, size_t maxLength);
void* readerFunc(readerArgs *args);
void* writerFunc(void *fd);
void clearResources(int fd);

pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t stdoutMutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t condition = PTHREAD_COND_INITIALIZER; 

int main()
{
	int fd = open("File", O_CREAT | O_RDWR | O_TRUNC, 00777);
	pthread_t readers[READERS_COUNT], writer;
	readerArgs args[READERS_COUNT];

	if (fd == -1)
	{
		printf("Error: %s", strerror(errno));
		clearResources(fd);
		return -1;
	}


	errno = pthread_create(&writer, NULL, writerFunc, &fd);
	
	if (errno != 0)
	{
		printf("Error: %s\n", strerror(errno));
		clearResources(fd);
		return -1;
	}

	for (int i = 0; i < READERS_COUNT; i++)
	{
		args[i].fd = fd;
		args[i].number = i + 1;
		
		errno = pthread_create(readers + i, NULL, 
						(void* (*)(void*)) readerFunc, args + i);
		
		if (errno != 0)
		{
			printf("Error: %s\n", strerror(errno));
			clearResources(fd);
			return -1;
		}
	}

	pthread_join(writer, NULL);
	
	for (int i = 0; i < READERS_COUNT; i++)
	pthread_join(readers[i], NULL);

	clearResources(fd);

	return 0;
}

ssize_t pgetLine(int fd, char *str, size_t maxLength, off_t offset)
{
	char buf[maxLength];
	ssize_t retVal = pread(fd, buf, maxLength, offset);
	size_t newLinePos;	

	if (retVal == -1)
	return -1;

	newLinePos = strcspn(buf, "\n");

	if (newLinePos == strlen(buf))
	return 0;
	
	newLinePos++;
	strncpy(str, buf, newLinePos);

	return newLinePos;
}

void* readerFunc(readerArgs *args)
{
	char str[STR_SIZE]; 
	int fd = args->fd;
	ssize_t number = args->number, bytesCount;
	off_t offset = 0;
	
	do
	{
		pthread_mutex_lock(&fileMutex);
		
		while ((bytesCount = pgetLine(fd, str, STR_SIZE, offset)) <= 0)
		{	
		
			pthread_mutex_lock(&stdoutMutex);
			printf("Thread%lu wait for data...\n", number);
			pthread_mutex_unlock(&stdoutMutex);
			
			pthread_cond_wait(&condition, &fileMutex);
		}
		
		pthread_mutex_unlock(&fileMutex);

		offset += bytesCount;
		
		if (strncmp(str, "END", 3) == 0)	
		break;

		pthread_mutex_lock(&stdoutMutex);
		printf("Thread%lu %s", number, str);
		fflush(stdout);
		pthread_mutex_unlock(&stdoutMutex);

		if (offset > STR_SIZE*LINES_COUNT)
		return NULL;
	}
	while (true);
		
	pthread_mutex_lock(&stdoutMutex);
	printf("Thread%lu END\n", number);
	fflush(stdout);
	pthread_mutex_unlock(&stdoutMutex);

	return NULL;
}

void* writerFunc(void *fd)
{
	int _fd = *(int*)fd;
	char str[STR_SIZE], timeStr[50]; 
	
	for (int i = 1; i < LINES_COUNT + 1; i++)
	{
		getCurrentTime(timeStr, 50);
		sprintf(str, "Line %d Current time %s\n", i, timeStr);
		
		pthread_mutex_lock(&fileMutex);
		
		write(_fd, str, strlen(str));

		pthread_cond_broadcast(&condition);
		pthread_mutex_unlock(&fileMutex);
	}

	pthread_mutex_lock(&fileMutex);
	write(_fd, "END\n", 4);
	
	pthread_cond_broadcast(&condition);
	pthread_mutex_unlock(&fileMutex);

	return NULL;
}

void getCurrentTime(char *str, size_t maxLength)
{
	static struct timeval timer;
	
	gettimeofday(&timer, NULL);
	strftime(str, maxLength, "%T.", localtime(&timer.tv_sec));
	sprintf(str + strlen(str), "%ld", timer.tv_usec);
}

void clearResources(int fd)
{
	close(fd);
	pthread_mutex_destroy(&fileMutex);
	pthread_mutex_destroy(&stdoutMutex);
}
