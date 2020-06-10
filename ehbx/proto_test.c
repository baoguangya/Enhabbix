#include<stdarg.h>
#include<stdio.h>
#include<string.h>
#include<stdint.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<fcntl.h>
#include<stdlib.h>
#include "zlib.h"

uint32_t big_endian(uint32_t data){
  unsigned char buf[4];
  memcpy(buf, &data, sizeof(buf));

  data = (uint32_t)buf[3];
  data <<= 8;
  data |= (uint32_t)buf[2];
  data <<= 8;
  data |= (uint32_t)buf[1];
  data <<= 8;
  data |= (uint32_t)buf[0];
  return data;
}

uint32_t little_endian(uint32_t data){
  unsigned char buf[4];
  
  buf[0] = (unsigned char)data;
  data >>= 8;
  buf[1] = (unsigned char)data;
  data >>= 8;
  buf[2] = (unsigned char)data;
  data >>= 8;
  buf[3] = (unsigned char)data;
  data >>= 8;
  memcpy(&data, buf, sizeof(buf));

  return data;
}

int main(int argc, char **argv){

  unsigned char hd[4] = {'Z','B','X','D'};
  char buf_write[65536], buf_read[65536];
  uint32_t len, remain;
  int i=0, offset=0;
  char *arg;
  int s, conn, res, uncomp, comp;
  struct sockaddr_in addr;
  char *ip, *port;
  char out[65536];
  uLongf sz_out,sz_comp;
  Bytef *buf_comp;

  ip = argv[1];
  if (argc < 3){
    arg = "";
  } else {
    arg = argv[2];
  }

  memcpy(buf_write + offset,hd,4);
  offset += 4;

  if(arg[0] == ' '){
    buf_write[offset++]=(unsigned char)3;
  } else {
    buf_write[offset++]=(unsigned char)1;
  }

  if (arg[0] == ' '){
    arg += 1;
    sz_comp = compressBound(strlen(arg));
    buf_comp = (Bytef *)malloc(sz_comp);
    comp = compress(buf_comp, &sz_comp, (Bytef *)arg,strlen(arg));
    if (Z_OK == comp){
      len = little_endian((uint32_t)sz_comp);
      memcpy(buf_write + offset,&len,sizeof(len));
      offset += sizeof(len);
    
      remain = little_endian((uint32_t)strlen(arg));
      memcpy(buf_write + offset,&remain,sizeof(remain));
      offset += sizeof(remain);

      memcpy(buf_write + offset, buf_comp, sz_comp);
      offset += sz_comp;
    } else {
      printf("error ocurred while compressing the message\n");
      exit(1);
    }
  } else {
    len = little_endian(strlen(arg));
    memcpy(buf_write + offset,&len,sizeof(len));
    offset += sizeof(len);

    remain = little_endian(0);
    memcpy(buf_write + offset,&remain,sizeof(remain));
    offset += sizeof(remain);
    memcpy(buf_write + offset, arg, strlen(arg));
    offset += strlen(arg);
  }

  printf("====================message to send==================================\n");
  
  for(i = 0; i < offset; i++){
    if (i >= 4 && i < 5 + sizeof(len) + sizeof(remain)){
      printf("%4d",(unsigned char)buf_write[i]);
    } else {
      printf("%c",buf_write[i]);
    }
  }
  printf("\n====================================================================\n");

  s = socket(AF_INET, SOCK_STREAM , 0);
  addr.sin_family =  AF_INET;
  addr.sin_addr.s_addr = inet_addr(ip);
  addr.sin_port = htons(atoi(port));
  
  conn = connect(s,(struct sockaddr *)&addr,sizeof(addr));
  if (conn != 0){
    printf("error while connecting to socket\n");
    exit(1);
  }
  
  fcntl(s, F_SETFD, FD_CLOEXEC);
  
  res = send(s, buf_write, offset,0);
  if (res != offset){
    printf("error while sending message, sent %d of %d in total\n", res, offset);
    exit(1);
  }

  memset(buf_read,0,sizeof(buf_read));

  if((res = recv(s, buf_read, sizeof(buf_read),0)) < 0){
    printf("error while receiving data\n");
    exit(1);
  }
 
  if(buf_read[4] == 3){
    sz_out = big_endian(*((uint32_t*)buf_read + 9));
    uncomp = uncompress((Bytef *)out, &sz_out, (const Bytef *) (buf_read + 13), res - 13);
  
    if (uncomp == Z_OK){
      memcpy(buf_read + 13, out, sz_out);
    } else {
      printf("error while uncompressing received msg\n");
      exit(1);
    }
  } else {
    sz_out = res - 13;
  }

  printf("====================message received================================\n");  
  for(i=0; i < sz_out+13; i++){
    if(i >=4 && i < 13){
      printf("%4d",(unsigned char)buf_read[i]);
    } else {
      printf("%c",buf_read[i]);
    }
  }
  printf("\n====================================================================\n");

} 
