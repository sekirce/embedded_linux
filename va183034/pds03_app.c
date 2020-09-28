#include <stdio.h>
#include <stdlib.h> 
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sqlite3.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <signal.h>

#define PDS03_I2C_ADDR     (0x42)

pthread_mutex_t sql_mutex;
pthread_t sens_mutex;

sqlite3 *db;
char query[256];
char *error_msg = 0;
int rc;

struct periodic_info { 
	int timer_fd;
	unsigned long long wakeups_missed;
};

static int make_periodic(unsigned int period, struct periodic_info *info) 
{
	int ret;
	unsigned int ns;
 	unsigned int s;
	int fd;
	struct itimerspec itval;
     
	fd = timerfd_create(CLOCK_MONOTONIC, 0);
	info->wakeups_missed = 0;
	info->timer_fd = fd;
	s = period / 1000000;
 	ns = (period - (s * 1000000)) * 1000;
	itval.it_interval.tv_sec = s;// period in seconds
	itval.it_interval.tv_nsec = ns;// period in ns
	itval.it_value.tv_sec = s;// initial delay in seconds
	itval.it_value.tv_nsec = ns;// initial delay in ns
	ret = timerfd_settime(fd, 0, &itval, NULL);
	return ret;
}

static void wait_period(struct periodic_info *info) 
{
	unsigned long long missed;
	int ret;    
 	/* Wait for the next timer event. If we have missed any then number is written to "missed" */
	ret = read(info->timer_fd, &missed, sizeof(missed));
	info->wakeups_missed += missed;

	//printf("Missed wakeups: %d \n", info->wakeups_missed);
}

void sigintHandler(int sig_num) 
{ 
    /* Reset handler to catch SIGINT next time. 
       Refer http://en.cppreference.com/w/c/program/signal */
    signal(SIGINT, sigintHandler); 
    printf("\n Cannot be terminated using Ctrl+C \n"); 
    fflush(stdout); 
} 

void* i2c_handler()
{
	int file;
	int8_t buffer[4];
	int16_t data;
	float tmp;
	
	int period;
	int file_freq;
	char freq[10] = {0};
	
	file_freq = open("/sys/class/pds03/pds03mmsensor0/pds03_mmsensor_frequency", O_RDONLY);
	if(file_freq < 0) {
	    printf("can't open pds03mmsensor0 - frequency\n");
	}
	read(file_freq,freq,sizeof(freq));
	close(file_freq);
	
	if(strcmp(freq,"normal\n") == 0) {
		period = 2000000;  // period = 2 s, freq = 0.5 Hz
	} else if(strcmp(freq,"fast\n") == 0) {
		period = 1000000;  // period = 1 s, freq = 1 Hz
	} else {
		printf("Bad command for frequency\n");
		period = 2000000; // period = 2 s, freq = 0.5 Hz
	}
	
	struct periodic_info information;

	file = open("/dev/i2c-2",O_RDWR);
	if(file < 0)
		printf("Can't open i2c-2\n");
	
	ioctl(file,I2C_SLAVE, PDS03_I2C_ADDR);
	printf("%d\n",I2C_SLAVE);

	buffer[0] = 0x0;
	buffer[1] = 0x10;
	write(file,buffer,2);

	make_periodic(period, &information);

	while(1){
		wait_period(&information);

		buffer[0] = 0x3;
		write(file,buffer,1);
		read(file,buffer,1);
		printf("i2c buffer[0] %d\n",buffer[0]);
		data = 0;
		data += buffer[0];
		buffer[0] = 0x2;
		write(file,buffer,1);
		read(file,buffer,1);
		if(data < 0) 
			tmp = (float)(data) - (float)(buffer[0])/256;
		else 
			tmp = (float)(data) + (float)(buffer[0])/256;
		
		printf("i2c buffer[0] %f\n",tmp);
	
		pthread_mutex_lock(&sql_mutex);
		sprintf(query , "INSERT INTO Temperature(Time , Data ) VALUES (%u ,%f);", (unsigned)time(NULL), tmp);
		rc = sqlite3_exec(db,query,0,0,&error_msg);
		pthread_mutex_unlock(&sql_mutex);
	}
}

int main(void)
{
	int file;
	int ret;

	char buff[10] = {0};
	char val[10] = {0};
	float value;

	int rc;
	char *err_msg = 0; 
	
	/* Query string for making tables */
	char *sql = 	"DROP  TABLE IF  EXISTS  Samples;"
			"CREATE  TABLE  Samples(Id INT , Time INT , Data REAL);"
			"DROP  TABLE IF  EXISTS  Temperature;"
			"CREATE  TABLE  Temperature(Id INT , Time INT , Data REAL);";
	int sens_value, sens_file, directionfd;
	int sysfs_fd;
	struct pollfd poll_file;

	// Open db and create tables
	rc = sqlite3_open("/www/test.db",&db);
	rc = sqlite3_exec(db,sql,0,0,&err_msg);

	// Define handler for SIGINT
	signal(SIGINT, sigintHandler);

	// Mutex initialization 
	pthread_mutex_init(&sql_mutex,NULL);

	// Starting thread for I2C
	pthread_create(&sens_mutex,NULL,&i2c_handler,NULL);

	// Enabling MMSENSOR
	sens_file = open("/sys/class/pds03/pds03mmsensor0/pds03_mmsensor_enabling", O_WRONLY);
	if(sens_file < 0) {
		printf("can't open pds03mmsensor0 - enabling component\n");
		return -1;
	}

	write(sens_file,"1",2);
	close(sens_file);
	printf("Component enabled\n");

	sens_file = open("/sys/class/pds03/pds03mmsensor0/pds03_mmsensor_enabling_interrupt", O_WRONLY);

	if(sens_file < 0) {
	    printf("can't open pds03mmsensor0 - enabling interrupt\n");
	    return -1;
	}

	write(sens_file,"1",2);
	close(sens_file);
	printf("Interrupt enabled\n");

	sens_file = open("/sys/class/pds03/pds03mmsensor0/pds03_mmsensor_resolution", O_RDWR);
	if(sens_file < 0) {
	    printf("can't open pds03mmsensor0 - resolution\n");
	    return -1;
	}

	write(sens_file,"fine\n",5);
	close(sens_file);
	printf("Change resolution\n");

	sens_file = open("/sys/class/pds03/pds03mmsensor0/pds03_mmsensor_frequency", O_RDWR);
	if(sens_file < 0) {
	    printf("can't open pds03mmsensor0 - frequency\n");
	    return -1;
	}

	write(sens_file,"normal\n",7);
	close(sens_file);
	printf("Change frequency\n");

	sens_file = open("/sys/class/pds03/pds03mmsensor0/pds03_mmsensor_data", O_RDONLY);
	if(sens_file < 0) {
	    printf("can't open pds03mmsensor0 - data\n");
	    return -1;
	}

	printf("Reading data\n");

	poll_file.fd = sens_file;
	poll_file.events = POLLPRI | POLLERR;

	while(1) {
		ret = poll(&poll_file, 1, -1);
		if(ret > 0) {
			// Read data 
			ret = read(sens_file,val,sizeof(val));
			printf("DATA = %s\n",val);
			value = atoi(val)/1000.0;
			
			// Enter data to tables
			pthread_mutex_lock(&sql_mutex);
			sprintf(query , "INSERT  INTO  Samples(Time , Data) VALUES  (%u,%f);",(unsigned)time(NULL), value);
			rc = sqlite3_exec(db, query , 0, 0, &err_msg);
			pthread_mutex_unlock(&sql_mutex);
			
			lseek(poll_file.fd,0,SEEK_SET);
		}
	}
	pthread_mutex_destroy(&sql_mutex);
	return 0;
}






