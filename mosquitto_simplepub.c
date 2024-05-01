// See https://github.com/LiamBindle/MQTT-C

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <mqtt.h>
#include "templates/posix_sockets.h"

void publish_callback(void** unused, struct mqtt_response_publish *published);
void* client_refresher(void* client);
void exit_example(int status, int sockfd, pthread_t *client_daemon);

void usage()
{
  printf("Usage: mosquitto_simplepub -t topic -m message [-h host] [-p port] [-u user_name] [-P password] [-q qos] [-i id]\n");
  printf(" -h : mqtt host to connect to. Defaults to localhost.\n");
  printf(" -i : id to use for this client. Defaults to mosquitto_simplepub.\n");
  printf(" -m : message payload to send.\n");
  printf(" -p : network port to connect to. Defaults to 1883.\n");
  printf(" -P : provide a password\n");
  printf(" -q : quality of service level to use for all messages. Defaults to 0.\n");
  printf(" -t : mqtt topic to publish to.\n");
  printf(" -u : provide a username\n");
  printf(" --help: display this message.\n");

  exit(EXIT_FAILURE);
}

int main(int argc, const char *argv[])
{
    const char *addr = "localhost";
    const char *port = "1883";
    const char *topic = NULL;
    const char *message = NULL;
    const char *user_name = NULL;
    const char *password = NULL;
    const char *id = "mosquitto_simplepub";
    int qos = MQTT_PUBLISH_QOS_0;
    int i;

    for (i = 1; i < argc - 1; i++)
    {
        if (strcmp(argv[i], "-h") == 0)
          addr = argv[i + 1];
        else if (strcmp(argv[i], "-p") == 0)
          port = argv[i + 1];
        else if (strcmp(argv[i], "-t") == 0)
          topic = argv[i + 1];
        else if (strcmp(argv[i], "-m") == 0)
          message = argv[i + 1];
        else if (strcmp(argv[i], "-q") == 0)
          qos = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-u") == 0)
          user_name = argv[i + 1];
        else if (strcmp(argv[i], "-P") == 0)
          password = argv[i + 1];
        else if (strcmp(argv[i], "-i") == 0)
          id = argv[i + 1];
        else if (strcmp(argv[i], "--help") == 0)
          usage();
    }

    if (topic == NULL || message == NULL)
      usage();

    /* open the non-blocking TCP socket (connecting to the broker) */
    int sockfd = open_nb_socket(addr, port);
    if (sockfd == -1) {
        perror("Failed to open socket: ");
        exit_example(EXIT_FAILURE, sockfd, NULL);
    }
    /* setup a client */
    struct mqtt_client client;
    uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
    uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
    mqtt_init(&client, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
    mqtt_connect(&client, id, NULL, NULL, 0, user_name, password, 0, 400);
    /* check that we don't have any errors */
    if (client.error != MQTT_OK) {
        fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
        exit_example(EXIT_FAILURE, sockfd, NULL);
    }
    /* start a thread to refresh the client (handle egress and ingree client traffic) */
    pthread_t client_daemon;
    if(pthread_create(&client_daemon, NULL, client_refresher, &client)) {
        fprintf(stderr, "Failed to start client daemon.\n");
        exit_example(EXIT_FAILURE, sockfd, NULL);
    }

    mqtt_publish(&client, topic, message, strlen(message), qos);
    /* check for errors */
    if (client.error != MQTT_OK) {
        fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
        exit_example(EXIT_FAILURE, sockfd, &client_daemon);
    }

    /* disconnect */
    sleep(1);
    /* exit */
    exit_example(EXIT_SUCCESS, sockfd, &client_daemon);
}
void exit_example(int status, int sockfd, pthread_t *client_daemon)
{
    if (sockfd != -1) close(sockfd);
    if (client_daemon != NULL) pthread_cancel(*client_daemon);
    exit(status);
}
void publish_callback(void** unused, struct mqtt_response_publish *published)
{
    /* not used in this example */
}
void* client_refresher(void* client)
{
    while(1)
    {
        mqtt_sync((struct mqtt_client*) client);
        usleep(100000U);
    }
    return NULL;
}
