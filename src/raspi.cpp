#include "raspi.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <memory>
#include <pthread.h>

//camera start
#include <linux/videodev2.h>
//camera end


#include "protocol.h"

#define __DEBUG
#include "util.h"


static int sockfd = 0;
static int camera_status = CAMERA_START;
static int remotefd = 0;
const char* dev_name = "/dev/video1";

struct cap_handle *caphandle = NULL;
struct cvt_handle *cvthandle = NULL;
struct enc_handle *enchandle = NULL;
struct pac_handle *pachandle = NULL;
struct net_handle *nethandle = NULL;
struct tms_handle *tmshandle = NULL;

struct cap_param capp;
struct cvt_param cvtp;
struct enc_param encp;
struct pac_param pacp;
struct net_param netp;
struct tms_param tmsp;

int init_camera()
{
    U32 vfmt = V4L2_PIX_FMT_YUYV;
    U32 ofmt = V4L2_PIX_FMT_YUV420;

    // set default values
    capp.dev_name = const_cast<char *>(dev_name);
    capp.width = 640;
    capp.height = 480;
    capp.pixfmt = vfmt;
    capp.rate = 15;

    cvtp.inwidth = 640;
    cvtp.inheight = 480;
    cvtp.inpixfmt = vfmt;
    cvtp.outwidth = 640;
    cvtp.outheight = 480;
    cvtp.outpixfmt = ofmt;

    encp.src_picwidth = 640;
    encp.src_picheight = 480;
    encp.enc_picwidth = 640;
    encp.enc_picheight = 480;
    encp.chroma_interleave = 0;
    encp.fps = 15;
    encp.gop = 12;
    encp.bitrate = 1000;

    pacp.max_pkt_len = 1400;
    pacp.ssrc = 1234;

    netp.serip = NULL;
    netp.serport = -1;
    netp.type = UDP;

    tmsp.startx = 10;
    tmsp.starty = 10;
    tmsp.video_width = 640;
    tmsp.factor = 0;

    caphandle = capture_open(capp);
    if (!caphandle)
    {
        printf("--- Open capture failed\n");
        return -1;
    }

    cvthandle = convert_open(cvtp);
    if (!cvthandle)
    {
        printf("--- Open convert failed\n");
        return -1;
    }

    enchandle = encode_open(encp);
    if (!enchandle)
    {
        printf("--- Open encode failed\n");
        return -1;
    }

    pachandle = pack_open(pacp);
    if (!pachandle)
    {
        printf("--- Open pack failed\n");
        return -1;
    }

    /*
    if ((stage & 0b00001000) != 0)
    {
        if (netp.serip == NULL || netp.serport == -1)
        {
            printf(
                    "--- Server ip and port must be specified when using network\n");
            return -1;
        }

        nethandle = net_open(netp);
        if (!nethandle)
        {
            printf("--- Open network failed\n");
            return -1;
        }
    }
    */
    // timestamp try
    tmshandle = timestamp_open(tmsp);
    return 0;
}

int send_pkg(int sockfd, Pkg &pkg)
{
    int byte_sent = 0;
    int len = pkg.stHead.len + sizeof(pkg.stHead);
    while(1)
    {
        int s = send(sockfd, (char*)(&pkg) + byte_sent, len - byte_sent, 0);
        if(s >= 0)
        {
            byte_sent += s;
            //log_debug("send packet %d bytes len %d", s, len);
        }
        else
        {
            log_debug("in send_pkg %s", strerror(errno));
            return -1;
        }

        if(byte_sent == len)
        {
            //log_debug("packet fully sent %d bytes sent cmd %d len %d", byte_sent, pkg.stHead.cmd, pkg.stHead.len);
            break;
        }
    }
    return 0;
}

int recv_pkg(int sockfd, Pkg &pkg)
{
    pkg.construct();
    int len = 0;
    int r = 0;
    int pkglen = sizeof(pkg.stHead);
    while(1)
    {
        r = recv(sockfd, (char*)(&pkg) + len, pkglen - len, 0);
        if (r > 0)
        {
            //log_debug("read head %d bytes read",r)
            len += r;
        }
        else
        {
            log_debug("recv head error");
            return -1;
        }

        if (len == pkglen)
        {
            break;
        }
    }

    pkglen += pkg.stHead.len;
    while(1)
    {
        r = recv(sockfd, (char*)(&pkg) + len, pkglen - len, 0);
        if (r > 0)
        {
            len += r;
            //log_debug("client %d bytes recvd total %d pkglen %d", r, len, pkglen);
        }
        else
        {
            printf("recv pkg failed\n");
            return -1;
        }

        if (len == pkglen)
        {
            //printf("recv pkg succ\n");
            break;
        }
    }

    return 0;
}

void* camera_thread(void *arg)
{
    init_camera();

    int sockfd = *(int*)arg;
    int ret;
    void *cap_buf, *cvt_buf, *hd_buf, *enc_buf, *pac_buf;
    int cap_len, cvt_len, hd_len, enc_len, pac_len;
    enum pic_t ptype;
    struct timeval ctime, ltime;
    unsigned long fps_counter = 0;
    int sec, usec;
    double stat_time = 0;

    capture_start(caphandle);		// !!! need to start capture stream!
    gettimeofday(&ltime, NULL);
    while (camera_status == CAMERA_START)
    {
        /*if(false)		// print fps
        {
            gettimeofday(&ctime, NULL);
            sec = ctime.tv_sec - ltime.tv_sec;
            usec = ctime.tv_usec - ltime.tv_usec;
            if (usec < 0)
            {
                sec--;
                usec = usec + 1000000;
            }
            stat_time = (sec * 1000000) + usec;		// diff in microsecond

            if (stat_time >= 1000000)    // >= 1s
            {
                printf("\n*** FPS: %ld\n", fps_counter);

                fps_counter = 0;
                ltime = ctime;
            }
            fps_counter++;
        }*/

        ret = capture_get_data(caphandle, &cap_buf, &cap_len);
        if (ret != 0)
        {
            if (ret < 0)		// error
            {
                printf("--- capture_get_data failed\n");
                break;
            }
            else	// again
            {
                usleep(10000);
                continue;
            }
        }
        if (cap_len <= 0)
        {
            printf("!!! No capture data\n");
            continue;
        }

        // convert
        if (capp.pixfmt == V4L2_PIX_FMT_YUV420)    // no need to convert
        {
            cvt_buf = cap_buf;
            cvt_len = cap_len;
        }
        else	// do convert: YUYV => YUV420
        {
            ret = convert_do(cvthandle, cap_buf, cap_len, &cvt_buf, &cvt_len);
            if (ret < 0)
            {
                printf("--- convert_do failed\n");
                break;
            }
            if (cvt_len <= 0)
            {
                printf("!!! No convert data\n");
                continue;
            }
        }

        // here add timestamp
        timestamp_draw(tmshandle, (unsigned char*)cvt_buf);


        // encode
        // fetch h264 headers first!
        while ((ret = encode_get_headers(enchandle, &hd_buf, &hd_len, &ptype))
               != 0)
        {
            // pack headers
            pack_put(pachandle, hd_buf, hd_len);
            while (pack_get(pachandle, &pac_buf, &pac_len) == 1)
            {
                // network todo
                /*Pkg pkg;
                pkg.construct();
                pkg.stHead.cmd = PKG_STREAM_DATA_NTF;
                pkg.stHead.len = pac_len;
                memcpy(pkg.stBody.stStreamDataNtf.buf, pac_buf, pac_len);
                printf("send camera data to client len %d\n", pac_len);
                int iRet = send_pkg(sockfd, pkg);
                if (iRet != 0)
                {
                    printf("send pkg to client error");
                    return NULL;
                }*/
                log_debug("h264 head len %d ------------------", pac_len);
                int byte_left = pac_len;
                Pkg pkg;
                pkg.construct();
                pkg.stHead.cmd = PKG_STREAM_DATA_NTF;
                while(byte_left > 0)
                {
                    if(byte_left >= 1414)
                    {
                        pkg.stHead.len = 1414;
                        memcpy(pkg.stBody.stStreamDataNtf.buf, (char*)pac_buf + pac_len - byte_left, pkg.stHead.len);
                        byte_left -= 1414;
                    }
                    else
                    {
                        pkg.stHead.len = byte_left;
                        memcpy(pkg.stBody.stStreamDataNtf.buf, (char*)pac_buf + pac_len - byte_left, pkg.stHead.len);
                        byte_left = 0;
                    }

                    memcpy(pkg.stBody.stStreamDataNtf.buf, (char*)pac_buf + pac_len - byte_left, pkg.stHead.len);
                    printf("send camera data to client len %d\n", pac_len);
                    int iRet = send_pkg(sockfd, pkg);
                    if (iRet != 0)
                    {
                        printf("send pkg to client error\n");
                        return NULL;
                    }
                }
            }
        }

        ret = encode_do(enchandle, cvt_buf, cvt_len, &enc_buf, &enc_len,
                        &ptype);
        if (ret < 0)
        {
            printf("--- encode_do failed\n");
            break;
        }
        if (enc_len <= 0)
        {
            printf("!!! No encode data\n");
            continue;
        }

        log_debug("h264 stream len %d", enc_len);
        int byte_left = enc_len;
        Pkg pkg;
        pkg.construct();
        pkg.stHead.fd = remotefd;
        pkg.stHead.cmd = PKG_STREAM_DATA_NTF;
        while(byte_left > 0)
        {
            if(byte_left >= 1414)
            {
                pkg.stHead.len = 1414;
                memcpy(pkg.stBody.stStreamDataNtf.buf, (char*)enc_buf + enc_len - byte_left, pkg.stHead.len);
                byte_left -= 1414;
            }
            else
            {
                pkg.stHead.len = byte_left;
                memcpy(pkg.stBody.stStreamDataNtf.buf, (char*)enc_buf + enc_len - byte_left, pkg.stHead.len);
                byte_left = 0;
            }

            //printf("send camera data to client len %d byte left %d\n", pkg.stHead.len, byte_left);
            int iRet = send_pkg(sockfd, pkg);
            if (iRet != 0)
            {
                printf("send pkg to client error\n");
                return NULL;
            }
        }

        // pack
        /*pack_put(pachandle, enc_buf, enc_len);
        while (pack_get(pachandle, &pac_buf, &pac_len) == 1)
        {
            // network todo
            Pkg pkg;
            pkg.construct();
            pkg.stHead.cmd = PKG_STREAM_DATA_NTF;
            pkg.stHead.len = pac_len;
            memcpy(pkg.stBody.stStreamDataNtf.buf, pac_buf, pac_len);
            printf("send camera data to client len %d\n", pac_len);
            int iRet = send_pkg(sockfd, pkg);
            if (iRet != 0)
            {
                printf("send pkg to client error\n");
                return NULL;
            }
        }*/
    }

    capture_stop(caphandle);
    pack_close(pachandle);
    encode_close(enchandle);
    convert_close(cvthandle);
    capture_close(caphandle);
    timestamp_close(tmshandle);
    return NULL;
}



int connect2server()
{
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP, &servaddr.sin_addr);
    int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if (ret != 0)
    {
        return -1;
    }
    return sockfd;
}

int main()
{

    signal(SIGPIPE, SIG_IGN);

    int sockfd = connect2server();
    printf("connect server sockfd %d\n", sockfd);

    //封装数据包
    Pkg pkg;
    pkg.construct();
    pkg.stHead.cmd = PKG_REGISTER_CLIENT_REQ;
    pkg.stHead.len = sizeof(pkg.stBody.stRegisterClientReq);
    pkg.stBody.stRegisterClientReq.clientType = CTYPE_RASPI;
    int iRet = send_pkg(sockfd, pkg);
    if (iRet != 0)
    {
        printf("send pkg error\n");
        return -1;
    }

    log_debug("sent shakehands req to server\n");

    Pkg recvpkg;
    iRet = recv_pkg(sockfd, recvpkg);
    if (iRet != 0 || recvpkg.stHead.cmd != PKG_REGISTER_CLIENT_RES)
    {
        printf("recv pkg error iRet %d cmd %d\n", iRet, recvpkg.stHead.cmd);
        return -1;
    }

    log_debug("shakehands succ!");

    while(1)
    {
        //wait for send data

        recvpkg.construct();
        iRet = recv_pkg(sockfd, recvpkg);
        if (iRet != 0)
        {
            printf("recv pkg error\n");
            return -1;
        }

        if (recvpkg.stHead.cmd == PKG_RESTART_CLIENT_NTF)
        {
            log_debug("stop pthread");
            camera_status = CAMERA_STOP;
            continue;
        }

        if (recvpkg.stHead.cmd == PKG_START_SEND_DATA_NTF)
        {
            log_debug("recv send data ntf");
            camera_status = CAMERA_START;
            remotefd = recvpkg.stBody.stStartSendDataNtf.reserve;
        }
        else
        {
            continue;
        }

        //启动子线程 采集并发送摄像头数据
        pthread_t pid = 0;
        int iRet = pthread_create(&pid, NULL, camera_thread, &sockfd);
        if (iRet < 0)
        {
            printf("thread create error %d %s\n", iRet, strerror(errno));
            return -1;
        }

        log_debug("wait for new client");
    }
}

