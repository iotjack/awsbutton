#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <memory.h>
#include <sys/time.h>
#include <limits.h>

#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_interface.h"
#include "aws_iot_config.h"
#include "mraa.h"
#define N 10
#define step 5

static volatile int counter = 0;
static volatile int oldcounter = 0;
mraa_gpio_context x;

char certDirectory[PATH_MAX + 1] = "../../certs";
char HostAddress[255] = AWS_IOT_MQTT_HOST;
uint32_t port = AWS_IOT_MQTT_PORT;


send_iot_command(int delta) {
	IoT_Error_t rc = NONE_ERROR;
	int32_t i = 0;

	char rootCA[PATH_MAX + 1];
	char clientCRT[PATH_MAX + 1];
	char clientKey[PATH_MAX + 1];
	char CurrentWD[PATH_MAX + 1];
	char cafileName[] = AWS_IOT_ROOT_CA_FILENAME;
	char clientCRTName[] = AWS_IOT_CERTIFICATE_FILENAME;
	char clientKeyName[] = AWS_IOT_PRIVATE_KEY_FILENAME;
	char buffer [20];
  	time_t rawtime;
  	struct tm * timeinfo;

  	time (&rawtime);
 	timeinfo = localtime (&rawtime);
  	strftime(buffer,20,"%D",timeinfo);

	getcwd(CurrentWD, sizeof(CurrentWD));
	sprintf(rootCA, "%s/%s/%s", CurrentWD, certDirectory, cafileName);
	sprintf(clientCRT, "%s/%s/%s", CurrentWD, certDirectory, clientCRTName);
	sprintf(clientKey, "%s/%s/%s", CurrentWD, certDirectory, clientKeyName);

	MQTTConnectParams connectParams = MQTTConnectParamsDefault;

	connectParams.KeepAliveInterval_sec = 10;
	connectParams.isCleansession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	connectParams.pClientID = "iot_button";
	connectParams.pHostURL = HostAddress;
	connectParams.port = port;
	connectParams.isWillMsgPresent = false;
	connectParams.pRootCALocation = rootCA;
	connectParams.pDeviceCertLocation = clientCRT;
	connectParams.pDevicePrivateKeyLocation = clientKey;
	connectParams.mqttCommandTimeout_ms = 2000;
	connectParams.tlsHandshakeTimeout_ms = 5000;
	connectParams.isSSLHostnameVerify = true;// ensure this is set to true for production
	rc = aws_iot_mqtt_connect(&connectParams);
	if (NONE_ERROR != rc) {
		ERROR("Error(%d) connecting to %s:%d", rc, connectParams.pHostURL, connectParams.port);
	}


	MQTTMessageParams Msg = MQTTMessageParamsDefault;
	Msg.qos = QOS_0;
	char cPayload[100];
	sprintf(cPayload, "{\"delta\":\"%d\", \"localtime\":\"%s\"}",delta, buffer);

	MQTTPublishParams Params = MQTTPublishParamsDefault;
	Params.pTopic = "topic/wait";
	Msg.pPayload = (void *) cPayload;
	Msg.PayloadLen = strlen(cPayload) + 1;
	Params.MessageParams = Msg;
	rc = aws_iot_mqtt_publish(&Params);
	rc = aws_iot_mqtt_disconnect();
	return rc;
}


void singleclick(void) {
    	printf("single click - increase wait time by %d minutes\n", step);
	send_iot_command(step);
}

void doubleclick() {
    printf("double click - decrease wait time by %d minutes\n", step);
    send_iot_command(-step);
}

int timer_running = 0; 
static volatile int ps[N]; 

void detectDoubleClick(void) {
    //looking for 1 ... 0 ..
    int i = 0;
//    for (i= 0; i<N; i++) printf("%d ", ps[i]);
//    printf("\n");
    int first1 = 0;
    while (ps[first1] ==0 && first1 < N )
    {
	first1++;
    }
    int anyzero = 1;
    for (i = first1; i<N; i++)
        anyzero = ps[i]*anyzero;
    if (anyzero == 0)
   	doubleclick();
    else
	singleclick();
}

void timer_handler(void) {
      ps[counter] = mraa_gpio_read(x);
      //printf("timer tick; gpio=%d\n", ps[counter]);
      ++counter;
      if (counter >= N )
      {
        stop_timer();
        timer_running = 0;
	counter=0;
 	detectDoubleClick();
      } 
}
void interrupt(void* args) {
    if (timer_running == 0)
    {
       //printf("isr - start timer\n");
       if(start_timer(50, &timer_handler))
       {
            printf("\n timer error\n");
            return;
       }
       timer_running = 1;
    }
}


int main() {
    mraa_init(); // mraa_gpio_context x;
    x = mraa_gpio_init(15);
    if (x == NULL) {
        return 1;
    }
    mraa_gpio_dir(x, MRAA_GPIO_IN);
    mraa_gpio_edge_t edge = MRAA_GPIO_EDGE_RISING;
	//BOTH, FALLING, or RISING
    mraa_gpio_isr(x, edge, &interrupt, NULL);
    for (;;) {
        if (counter != oldcounter) {
            //fprintf(stdout, "timeout counter == %d\n", counter);
	    oldcounter = counter;
        }
        sleep(1);
    }
    mraa_gpio_close(x);
    return MRAA_SUCCESS;
}

