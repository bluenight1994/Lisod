#!/usr/bin/env python
import datetime
import requests
import os
import time
from grader import grader, tester
import hashlib
import random
import lowlevelhttptests
from subprocess import Popen, PIPE, STDOUT
import os.path
import socket

from http_parser.pyparser import HttpParser
import logging
try:
    import http.client as http_client
except ImportError:
    # Python 2
    import httplib as http_client
http_client.HTTPConnection.debuglevel = 1

# You must initialize logging, otherwise you'll not see debug output.
logging.basicConfig()
logging.getLogger().setLevel(logging.DEBUG)
requests_log = logging.getLogger("requests.packages.urllib3")
requests_log.setLevel(logging.DEBUG)
requests_log.propagate = True

BIN = "./liso"

MIME = {
    '.html': 'text/html',
    '.css': 'text/css',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.gif': 'image/gif',
    '': 'application/octet-stream'
}


class project1cp2tester(tester):

    def __init__(self, test_name, testsuit):
        super(project1cp2tester, self).__init__(test_name, testsuit)

    def test_kill(self):
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        print "kill it"
        self.testsuite.process.terminate()
        return

    def setUp(self):
        self.testsuite.port = random.randint(1025, 9999)
        self.start_server()
        time.sleep(3)

    def tearDown(self):
        self.test_kill()
        time.sleep(5)

    def test_using_select(self):
        print "Simple checker to tell if you are using select(). \
        We will check it manually later."
        p = Popen("grep -rq 'select' ./ ", \
            shell=True, stdout=PIPE, stderr=STDOUT)
        rc = p.wait()
        if rc != 0:
            self.testsuite.scores['use_select'] = 0
            return
        else:
            return

    def start_server(self):
        if self.testsuite.scores['test_make'] <= 0:
            self.skipTest("Failed to make. Skip this test")
        if self.testsuite.scores['use_select'] <= 0:
            self.skipTest("Select() is not used. Skip this test")
        # if not os.path.isfile("lisod"):
        #     if os.path.isfile("echo_server"):
        #         print "Your makefile should make a binary called lisod, \
        #                 not echo_server!"
        #     self.skipTest("lisod not found. Skip this test")

        print "Try to start server!"
        # cmd = '%s %d %d lisod.log %slisod.lock %s %s %s %s' % \
        #         (BIN, self.testsuite.port, self.testsuite.tls_port, \
        #         self.testsuite.tmp_dir, \
        #         self.testsuite.www[:-1], self.testsuite.cgi, \
        #         self.testsuite.priv_key, self.testsuite.cert)
        cmd = '%s %d ./lisod.log %s' % (BIN, self.testsuite.port, self.testsuite.www[:-1])
        print cmd
        fp = open(os.devnull, 'w')
        p = Popen(cmd.split(' '), stdout=fp, stderr=fp)
        print "Wait 2 seconds."
        time.sleep(2)
        if p.poll() is None:
            print "Server is running"
            self.testsuite.process = p
            self.testsuite.scores['server_start'] = 1
        else:
            raise Exception("server dies within 2 seconds!")

    def check_headers(self, response_type, headers, length_content, ext):
        res = 0
        if headers['Server'].lower() == 'liso/1.0':
            res += 0.2
        try:
            datetime.datetime.strptime(headers['Date'],\
             '%a, %d %b %Y %H:%M:%S %Z')
            res += 0.2
        except KeyError:
            print 'Bad Date header'
            pass
        except Exception:
            print 'Bad Date header: %s' % (headers['Date'])
            pass

        if int(headers._store['content-length'][1]) == length_content:
            res += 0.2

        if response_type == 'GET' or response_type == 'HEAD':
            header_set = {'connection', 'content-length', 'date', 'last-modified', 'server', 'content-type'}
            for item in header_set:
                if item in set(headers._store.keys()):
                    res += 0.1/len(header_set)


            # if headers._store['content-type'][1].lower() != MIME[ext]:
            #     print 'MIME got %s expected %s'\
            #      % (headers._store['content-type'][1].lower(), MIME[ext])
            if headers._store['content-type'][1].lower() == MIME[ext]\
                 or headers._store['content-type'][1].lower() == MIME['.html']:
                res +=0.1
            try:
                datetime.datetime.strptime(headers['last-modified'],\
                '%a, %d %b %Y %H:%M:%S %Z')
                res += 0.2
            except:
                print 'Bad last-modified header'
                pass
            return res
        elif response_type == 'POST':
            header_set = {'connection', 'content-length', 'date', 'server'}
            for item in header_set:
                if item in set(headers._store.keys()):
                    res += 0.1/len(header_set)
            return res
        else:
            self.fail('Unsupported Response Type...')



    def test_HEAD_headers(self):
        print '----- Testing Headers -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(1)
        for test in self.testsuite.tests:
            try:
                dummy_root, ext = os.path.splitext(test)
                req = test % self.testsuite.port
                response = requests.head(req, timeout=10.0)
                res = self.check_headers(response.request.method,
                                   response.headers,
                                   self.testsuite.tests[test][1],
                                   ext)
                self.testsuite.scores['test_HEAD_headers'] += res/len(self.testsuite.tests)
            except socket.timeout:
                pass

    def test_directory_traversal(self):
        print "-testing request-"
        print '----- Testing GET -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(5)
        req =  self.testsuite.directory_traversal['url'] % self.testsuite.port
        response = requests.get(req, timeout=10.0)
        contenthash = hashlib.sha256(response.content).hexdigest()
        if response.status_code != 200:
            # the test didn't return the /index.html file but still
            # protected the file ../index.html
            # Desired behavior: convert ../../index.html to /index.html
            self.testsuite.scores['test_directory_traversal'] = 0.5
            return
        self.pAssertEqual(contenthash, self.testsuite.directory_traversal['hash'][0])
        self.testsuite.scores['test_directory_traversal'] = 1

    def test_long_GET(self):
        print "-testing request-"
        print '----- Testing GET -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(3)
        req = self.testsuite.simple_URL % self.testsuite.port+ '?' +'t'*30000
        try:
            response = requests.get(req , timeout=5.0)
        except socket.timeout:
            print 'timeout'
            raise Exception('timeout')
        if response.status_code == 414 or response.status_code == 404:
            self.testsuite.scores['test_long_GET'] = 1
        else:
            self.pAssertEqual(response.status_code, 404)

    def test_long_POST(self):
        print '----- test_long_POST -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(1)
        req = self.testsuite.simple_URL % self.testsuite.port
        try:
            response = requests.post(req, data={'test':'t'*3000}, timeout=10.0)
            self.pAssertEqual(response.status_code, 200)
            # response = requests.post(req, data='t' * 120000, timeout=3.0)
            self.testsuite.scores['test_long_POST'] = 1
        except socket.timeout:
            pass

    def test_special_chars(self):
        print '----- test_special_chars -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(5)
        req = self.testsuite.special_URL % self.testsuite.port
        response = requests.get(req, timeout=10.0)
        self.pAssertEqual(response.status_code, 200)
        self.pAssertEqual(response.headers._store['content-length'][1], str(self.testsuite.special_chars['length']))
        self.pAssertEqual(response.content.decode('utf-8'), self.testsuite.special_chars['string'])
        self.testsuite.scores['test_special_chars'] = 1

    def test_multiple_clients(self):
        print '---- test_multiple_clients -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(1)
        if lowlevelhttptests.check_multi_clients('127.0.0.1', self.testsuite.port, 10, 10):
            self.testsuite.scores['test_multiple_clients'] = 1

    def test_wrong_HEADER(self):
        print '---- test_multiple_clients -----'
        time.sleep(5)
        if lowlevelhttptests.test_wrong_header('127.0.0.1', self.testsuite.port):
            self.testsuite.scores['test_wrong_HEADER'] = 1

    def test_wrong_content_length(self):
        print '---- test_wrong_content_length -----'
        if lowlevelhttptests.test_wrong_content_length('127.0.0.1', self.testsuite.port):
            self.testsuite.scores['test_wrong_content_length'] = 1

    def test_garbage_input(self):
        print '---- test_garbage_input -----'
        if lowlevelhttptests.test_garbage_input('127.0.0.1', self.testsuite.port):
            self.testsuite.scores['test_garbage_input'] = 1

    def test_check_content_type(self):
        for url, type in self.testsuite.content_types.iteritems():
            req = url % self.testsuite.port
            try:
                response = requests.get(url=req, timeout=3.0)
                if response.headers._store['content-type'][1] == type:
                    self.testsuite.scores['test_check_content_type'] += 1.0/len(self.testsuite.content_types)
            except:
                pass

    def test_HEAD(self):
        print '----- Testing HEAD -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(5)
        for test in self.testsuite.tests:
            try:
                req = test % self.testsuite.port
                response = requests.head(test % self.testsuite.port, timeout=10.0)
                self.pAssertEqual(200, response.status_code)
                self.testsuite.scores['test_HEAD'] += 1.0/len(self.testsuite.tests)
            except socket.timeout:
                pass
        if not lowlevelhttptests.check_correct_HEAD('127.0.0.1',\
                                                    self.testsuite.port):
            self.testsuite.scores['test_HEAD'] -= 0.5
            print "HEAD must not return Content!"

    def test_GET(self):
        print '----- Testing GET -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(5)
        for test in self.testsuite.tests:
            try:
                req = test % self.testsuite.port
                response = requests.get(req, timeout=10.0)
                contenthash = hashlib.sha256(response.content).hexdigest()
                self.pAssertEqual(200, response.status_code)
                # self.pAssertEqual(contenthash, self.testsuite.tests[test][0])
                self.testsuite.scores['test_GET'] += 1.0/len(self.testsuite.tests)
            except socket.timeout:
                print 'timeout for request: {0}'.format(test)
                pass

        err, res = lowlevelhttptests.check_correct_GET('127.0.0.1',\
                        self.testsuite.port)
        if res > 1:
            self.testsuite.scores['test_GET'] -= 0.3
            print "Received %d responses, should get only 1" % res
            if err:
                print "And, some of them reported error" % res

    def test_POST(self):
        print '----- Testing POST -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(5)
        for test in self.testsuite.tests:
            # for checkpoint 2, this should time out; we told them to
            # swallow the data and ignore
            try:
                response = requests.post(test % self.testsuite.port,\
                    data='dummy data', timeout=3.0)
            #except requests.exceptions.Timeout:
            except requests.exceptions.RequestException:
                #print 'timeout'
                continue
            except socket.timeout:
                #print 'socket.timeout'
                continue
            # if they do return something, make sure it's OK
            self.pAssertEqual(200, response.status_code)
            self.testsuite.scores['test_POST'] += 1.0/len(self.testsuite.tests)

    def test_bad(self):
        print '----- Testing Bad Requests-----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(5)
        for test in self.testsuite.bad_tests:
            response = requests.head(test % self.testsuite.port, timeout=3.0)
            self.pAssertEqual(404, response.status_code)
        self.testsuite.scores['test_bad'] = 1

    def test_big(self):
        print '----- Testing Big file -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(3)
        for test in self.testsuite.big_tests:
            response = requests.get(test % self.testsuite.port, timeout=5.0)
            contenthash = hashlib.sha256(response.content).hexdigest()
            self.pAssertEqual(200, response.status_code)
            self.pAssertEqual(contenthash, self.testsuite.big_tests[test])
        self.testsuite.scores['test_big'] = 1

    def test_apache_bench(self):
        print '----- Testing Apache Bench with pipelining-----'
        #print 'Make sure your lisod accepts http/1.0 requests'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(3)
        cmd = '../apachebench/ab -kc 100 -n 40000\
        http://127.0.0.1:%d/index.html > ../tmp/ab.log'% self.testsuite.port
        self.pAssertEqual(0, os.system(cmd))
        cmd_error_rate = "cat ../tmp/ab.log | grep Failed | awk '{print $3}' "
        p = Popen(cmd_error_rate, shell=True, stdout=PIPE, stderr=STDOUT)
        out, dummy_err = p.communicate()
        try:
            errors = int(out)
            if errors > 40000:
                errors = 40000
        except ValueError:
            errors = 0
        print "ab test errors:%d" % errors
        self.testsuite.scores['test_apache_bench'] =\
            round((40000-errors)/40000.0, 2)

class project1cp2grader(grader):
    def __init__(self, checkpoint):
        super(project1cp2grader, self).__init__()
        self.process = None
        self.checkpoint = checkpoint
        self.tests = {
            'http://127.0.0.1:%d/index.html':
                ('f5cacdcb48b7d85ff48da4653f8bf8a7c94fb8fb43407a8e82322302ab13becd', 802),
            'http://127.0.0.1:%d/images/liso_header.png':
                ('abf1a740b8951ae46212eb0b61a20c403c92b45ed447fe1143264c637c2e0786', 17431),
            'http://127.0.0.1:%d/style.css':
                ('575150c0258a3016223dd99bd46e203a820eef4f6f5486f7789eb7076e46736a', 301)
        }

        self.bad_tests = [
            'http://127.0.0.1:%d/bad.html'
        ]

        self.content_types = {
            'http://127.0.0.1:%d/index.html':
                'text/html',
            'http://127.0.0.1:%d/images/liso_header.png':
                'image/png',
            'http://127.0.0.1:%d/style.css':
                'text/css'
        }

        self.directory_traversal = {
            'url': 'http://127.0.0.1:%d/../index.html',
            'hash': ('f5cacdcb48b7d85ff48da4653f8bf8a7c94fb8fb43407a8e82322302ab13becd', 802)
        }

        self.simple_URL = 'http://127.0.0.1:%d/index.html'

        self.special_URL = 'http://127.0.0.1:%d/special.html'

        self.special_chars = {'string': u'\u0985\u02d9\u02da\u2206\u7ab7', 'length': 13}

        self.big_tests = {
            'http://127.0.0.1:%d/big.html':
                'fa066c7d40f0f201ac4144e652aa62430e58a6b3805ec70650f678da5804e87b'
        }

    def prepareTestSuite(self):
        super(project1cp2grader, self).prepareTestSuite()
        self.suite.addTest(project1cp2tester('test_using_select', self))
        self.suite.addTest(project1cp2tester('test_HEAD_headers', self))
        self.suite.addTest(project1cp2tester('test_HEAD', self))
        self.suite.addTest(project1cp2tester('test_GET', self))
        self.suite.addTest(project1cp2tester('test_POST', self))
        self.suite.addTest(project1cp2tester('test_bad', self))
        self.suite.addTest(project1cp2tester('test_big', self))
        self.suite.addTest(project1cp2tester('test_directory_traversal', self))
        self.suite.addTest(project1cp2tester('test_long_GET', self))
        self.suite.addTest(project1cp2tester('test_long_POST', self))
        self.suite.addTest(project1cp2tester('test_special_chars', self))
        self.suite.addTest(project1cp2tester('test_multiple_clients', self))
        self.suite.addTest(project1cp2tester('test_wrong_HEADER', self))
        self.suite.addTest(project1cp2tester('test_garbage_input', self))
        self.suite.addTest(project1cp2tester('test_check_content_type', self))

        self.scores['use_select'] = 1
        self.scores['server_start'] = 0
        self.scores['test_HEAD_headers'] = 0
        self.scores['test_HEAD'] = 0
        self.scores['test_GET'] = 0
        self.scores['test_POST'] = 0
        self.scores['test_bad'] = 0
        self.scores['test_big'] = 0
        self.scores['test_directory_traversal'] = 0
        self.scores['test_long_GET'] = 0
        self.scores['test_long_POST'] = 0
        self.scores['test_special_chars'] = 0
        self.scores['test_multiple_clients'] = 0
        self.scores['test_wrong_HEADER'] = 0
        # self.scores['test_wrong_content_length'] = 0
        self.scores['test_garbage_input'] = 0
        self.scores['test_check_content_type'] = 0
        # self.scores[''] = 0

    def setUp(self):
        self.port = random.randint(1025, 9999)
        # self.port = 9999
        self.tls_port = random.randint(1025, 9999)
        self.tmp_dir = "./tmp/"
        self.priv_key = os.path.join(self.tmp_dir, 'grader.key')
        self.cert = os.path.join(self.tmp_dir, 'grader.crt')
        self.www = os.path.join(self.tmp_dir, 'www/')
        self.cgi = os.path.join(self.tmp_dir, 'cgi/cgi_script.py')
        print '\nUsing ports: %d,%d' % (self.port, self.tls_port)


if __name__ == '__main__':
    p1cp2grader = project1cp2grader("checkpoint-2")
    p1cp2grader.prepareTestSuite()
    p1cp2grader.setUp()
    results = p1cp2grader.runTests()
    p1cp2grader.reportScores()
