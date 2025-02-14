/*

如果对方恶意不接受文件，此方法会大量占用文件描述符。（后有解决方法，限制最大并发连接数）
满足几乎所有健壮性

*/

#include <muduo/net/TcpServer.h>
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

void onHighWaterMark(const TcpConnectionPtr &conn, size_t len)
{
    LOG_INFO << "HighWaterMark " << len;
}

const int kBufSize = 64 * 1024;
const char *g_file = NULL;

void onConnection(const TcpConnectionPtr &conn)
{
    LOG_INFO << "FileServer - " << conn->peerAddress().toIpPort() << " -> "
             << conn->localAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    if (conn->connected())
    {
        LOG_INFO << "FileServer - sending file " << g_file
                 << " to " << conn->peerAddress().toIpPort();

        conn->setHighWaterMarkCallback(onHighWaterMark, kBufSize + 1);
        FILE *fp = ::fopen(g_file, "rb");
        if (fp)
        {
            conn->setContext(fp); // 设置内容。这个内容可以是任何数据，主要是用着一个临时存储作用。
            char buf[kBufSize];
            size_t nread = ::fread(buf, 1, kBufSize, fp);
            conn->send(buf, nread); // 存在WriteCompleteCallBack,在此重复写文件。
        }
        else
        {
            conn->shutdown();
            LOG_INFO << "FileServer - no such file";
        }
    }
    else
    {
        if (!conn->getContext().empty())
        {
            FILE *fp = boost::any_cast<FILE *>(conn->getContext());
            if (fp)
            {
                ::fclose(fp);
            }
        }
    }
}

void onWriteComplete(const TcpConnectionPtr &conn)
{
    FILE *fp = boost::any_cast<FILE *>(conn->getContext());
    char buf[kBufSize];
    size_t nread = ::fread(buf, 1, kBufSize, fp);
    if (nread > 0)
    {
        conn->send(buf, nread);
    }
    else
    {
        ::fclose(fp);
        fp = NULL;
        conn->setContext(fp);
        conn->shutdown();
        LOG_INFO << "FileServer - done ";
    }
}

int main(int argc, char *argv[])
{
    LOG_INFO << "pid = " << getpid();
    if (argc > 1)
    {
        g_file = argv[1];

        EventLoop loop;
        InetAddress listenAddr(2021);
        TcpServer server(&loop, listenAddr, "FileServer");
        server.setConnectionCallback(onConnection);
        server.setWriteCompleteCallback(onWriteComplete);
        server.start();
        loop.loop();
    }
    else
    {
        fprintf(stderr, "Usage: %s file for downlode", argv[0]);
    }
}