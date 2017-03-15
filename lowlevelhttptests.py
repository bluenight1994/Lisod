#!/usr/bin/env python
import socket
from http_parser.pyparser import HttpParser

def check_correct_HEAD(host, port):
    #Check if HEAD only returns header but not body
    p = HttpParser()
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    rc_is_headers_complete = False
    rc_no_more_data = True
    try:
        s.connect((host, port))
        s.settimeout(1)
        s.send("HEAD /index.html HTTP/1.1\r\nHost: %s:%d\
            \r\nConnection:Keep-Alive\r\n\r\n" % (host, port))
        while True:
            data = s.recv(1024)

            if rc_is_headers_complete and data:
                rc_no_more_data = False
                break

            if not data:
                break

            recved = len(data)
            nparsed = p.execute(data, recved)
            assert nparsed == recved

            if p.is_headers_complete():
                rc_is_headers_complete = True

            if p.is_message_complete():
                break
    except socket.timeout:
        pass
    finally:
        s.close()
    return rc_is_headers_complete and rc_no_more_data

def check_correct_GET(host, port):
    #Check if only one response from each GET 
    error_reported = False
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    responses = 0
    buf_size = 1024
    data_all = ""
    try:
        s.connect((host, port))
        s.send("GET /index.html HTTP/1.1\r\nHost: %s:%d\
            \r\nConnection:Keep-Alive\r\n\r\n" % (host, port))
        while True:
            s.settimeout(1)
            data = s.recv(buf_size)
            data_all += data

            if not data:
                break
    except socket.timeout:
        pass
    finally:
        s.close()
    # print data_all
    # data_all = data_all[:-2]
    p = HttpParser()
    while len(data_all) > 0:
        nparsed = p.execute(data_all, len(data_all))
        if nparsed == 0:
            break
        if p.is_message_complete():
            responses += 1
            if nparsed < len(data_all):
                responses += 1 #more data
            if p.get_status_code() >= 400:
                error_reported = True
            p = HttpParser() # create another
        data_all = data_all[nparsed:]

    return error_reported, responses


def check_multi_clients(host, port, iterations, amount):

    n = amount
    sockets = list()

    for k in range(n):
        sockets.append(socket.socket(socket.AF_INET, socket.SOCK_STREAM))

    try:
        for k in range(n):
            sockets[k].connect((host, port))
            sockets[k].settimeout(10)
        for i in range(iterations):
            print "iteration: {0}".format(i)

            s_data = list()

            for k in range(n):
                sockets[k].send("HEAD /index.html HTTP/1.1\r\nHost: %s:%d\
                        \r\nConnection:Keep-Alive\r\n\r\n" % (host, port))
            for k in range(n):
                s_data.append(sockets[k].recv(1024))

            for k in range(n):
                p = HttpParser()
                recved = len(s_data[k])
                nparsed = p.execute(s_data[k], recved)
                assert nparsed == recved

            assert p.is_headers_complete() is True

    except socket.timeout:
        pass
    finally:
        for k in range(n):
            sockets[k].close()
    return True


def test_wrong_header(host, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)

    responses = 0
    buf_size = 1024
    data = ""
    try:
        s.connect((host, port))
        s.send("GET /index.html HTTP/0.1\r\nHost: %s:%d\
                \r\nConnection:Keep-Alive\r\n\r\n" % (host, port))
        data = s.recv(buf_size)
    except socket.timeout:
        pass
    finally:
        s.close()
    p = HttpParser()
    p.execute(data, len(data))
    assert p.get_status_code()== 505

    return True


def test_wrong_content_length(host, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)

    responses = 0
    buf_size = 1024
    data = ""
    try:
        s.connect((host, port))
        s.send("POST /index.html HTTP/1.1\r\nHost: %s:%d \
               \r\ncontent-length:1 \
                \r\nConnection:Keep-Alive\r\ns\r\n\r\n" % (host, port))
        data = s.recv(buf_size)
        print data
    except socket.timeout:
        pass
    finally:
        s.close()
    p = HttpParser()
    p.execute(data, len(data))
    assert p.get_status_code() == 505

    return True


def test_garbage_input(host, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)

    responses = 0
    buf_size = 1024
    data = ""
    try:
        s.connect((host, port))
        s.send("sadfsdfasdflasdfsadjfsadl;fkdsf\r\n\r\n")
        data = s.recv(buf_size)

    except socket.timeout:
        pass
    finally:
        s.close()
    p = HttpParser()
    p.execute(data, len(data))
    assert p.get_status_code()== 400

    return True


if __name__ == "__main__":
    print check_correct_HEAD("127.0.0.1", 6799)
    print check_correct_HEAD("eaufavor.info", 80)
    print check_correct_GET("127.0.0.1", 6799)
    print check_correct_GET("www.angio.net", 80)
