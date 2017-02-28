from __future__ import print_function
from future import standard_library
standard_library.install_aliases()
from builtins import next
from builtins import str
from builtins import range
from builtins import object
import sys
sys.path.append("../")
import gdp
from threading import Thread, Lock, Condition, current_thread
import threading
import time
import configparser

gdp.gdp_init()

class Datagen(object):
    """
    From 0 to max_val-1 inclusive
    """
    def __init__(self, max_val):
        self.max_val = max_val
        self.lock = Lock()
        self.cur_val = 0
        self.valid = True

    def get_max_val(self):
        return self.max_val

    def __next__(self):
        self.lock.acquire()
        to_ret = self.cur_val
        if to_ret != self.max_val:
            self.cur_val += 1
        else:
            self.valid = False
        self.lock.release()
        return self.cur_val, self.valid

class Logcontroller(object):
    def __init__(self, gdp_name):
        self.gcl_name = gdp.GDP_NAME(gdp_name)
        self.gcl_handle = gdp.GDP_GCL(self.gcl_name, gdp.GDP_MODE_RA)
        self.out = 0
        self.resp = 0

    def write(self, val):
        self.gcl_handle.append({"data": str(val)})

    def write_async(self, val):
        self.gcl_handle.append_async({"data": str(val)})

    def read(self, rec_num):
        return self.gcl_handle.read(rec_num)

    def read_async(self, rec_num):
        return self.gcl_handle.read_async(rec_num)

    def inc_out(self):
        self.out += 1

    def inc_resp(self):
        self.resp += 1

    def get_write_dif(self):
        return self.out-self.resp

    def get_handle(self):
        return self.gcl_handle

class Batch(object):
    def __init__(self, datagen, logcontroller, interval, batchname):
        self.dg = datagen
        self.lc = logcontroller
        self.interval = interval
        self.batchname = batchname

    def get_dg(self):
        return self.dg

    def get_lc(self):
        return self.lc

    def get_lock(self):
        return self.lock

    def get_interval(self):
        return self.interval

    def get_batchname(self):
        return self.batchname

def write_batch_sync(batch):
    dg = batch.get_dg()
    lc = batch.get_lc()
    cur, valid = next(dg)
    while valid:
        print("Writing " + str(cur))
        lc.write(cur)
        lc.inc_out()
        lc.inc_resp()
        cur, valid = next(dg)

def write_batch_async(batch):
    dg = batch.get_dg()
    lc = batch.get_lc()
    cur, valid = next(dg)
    while valid:
        lc.write_async(cur)
        lc.inc_out()
        cur, valid = next(dg)
    print("Async write complete\n")

def read_batch_sync(batch):
    lc = batch.get_lc()
    dg = batch.get_dg()
    
    print(batch.get_batchname())
    for i in range(-dg.get_max_val(), 0):
        print("Reading " + str(i))
        datum = lc.read(i)
        data = int(datum['data'])
        if data > dg.get_max_val() or data < 0:
            print("Reading failed!")
            return
    print("Reading appears successful") 

# Not completed
def read_batch_async(batch):
    lc = batch.get_lc()
    dg = batch.get_dg()
    strt_out = lc.out
    strt_resp = lc.resp

    for i in range(dg.get_max_val()):
        datum = lc.read_async(i)
        lc.inc_out()
        if lc.get_write_dif() > batch.get_interval():
            wait_async_res(lc, 2)

    if lc.get_write_dif() > 0:
        wait_async_res(lc, 2)
    if lc.get_write_dif() > 0:
        print("read_batch_async:")
        print("asked for: " + (lc.out-strt_out))
        print("got: " + (lc.resp-strt_resp))

def read_batch_multiread(batch):
    lc = batch.get_lc()
    dg = batch.get_dg()
    lc.get_handle().multiread(-dg.get_max_val(), dg.get_max_val())
    t = {'tv_sec':0, 'tv_nsec':500*(10**6), 'tv_accuracy':0.0}
    cntr = 0
    while cntr < dg.get_max_val():
        event = gdp.GDP_GCL.get_next_event(t)
        if event is None:
            break
        if event["type"] == gdp.GDP_EVENT_EOS:
            continue
        datum = event["datum"]
        cntr += 1

    if cntr == dg.get_max_val():
        print("Multiread completed\n")
    else:
        print("Multiread failed\n")

def wait_async_res(lc, timeout):
    start = time.time()
    while lc.get_write_dif() > 0:
        event = lc.get_handle().get_next_event(None)    # Should add timeout
        if event["type"] == gdp.GDP_EVENT_DATA:
            lc.inc_resp()
        if time.time() - start > timeout:
            return

def read_batch_subscription(batch):
    lc = batch.get_lc()
    dg = batch.get_dg()

    lc.get_handle().subscribe(0, dg.get_max_val(), None)
    lc.out += dg.get_max_val()

    strt_out = lc.out
    strt_resp = lc.resp
    wait_async_res(lc, 10) 
    if dg.get_max_val() > (lc.resp-strt_resp):
        print("read_batch_async:")
        print("asked for: " + (lc.out-strt_out))
        print("got: " + (lc.resp-strt_resp))
    else:
        print("Reading successful!")

def runner(batch, condition, func, cnt):
    condition.acquire()
    condition.cntr += 1
    if condition.cntr == cnt:
        condition.notifyAll()
    else:
        condition.wait()
    condition.release()
    func(batch)

def main():
    # Lists must be same length
    print("Starting tests...")
    config = configparser.ConfigParser()
    config.read("config.ini")
    for section in config.sections():
        print("Starting section: " + str(section))
        batch_data = config[section]

        dg = Datagen(int(batch_data['num_records']))
        lc = Logcontroller(batch_data['router'])
        batch = Batch(dg, lc, int(batch_data['collection_interval']), section)

        running_threads = []
        num_readers = int(batch_data['n_readers'])
        dg.num_readers = num_readers
        if batch_data['subscribe'] == "true":
            condition1 = Condition()
            condition1.cntr = 0
            for i in range(num_readers):
                thread = Thread(target = runner,
                    args = (batch, condition1, read_batch_subscription, num_readers))
                running_threads.append(thread)
                thread.start()

        if batch_data['subscribe'] == "true":
            print("Subscriptions completed")

        condition2 = Condition()
        condition2.cntr = 0
        running_threads = []
        num_writers = int(batch_data['n_writers'])
        dg.num_writers = num_writers
        start = time.time()
        for i in range(num_writers):
            if batch_data['async_write'] == "true":
                thread = Thread(target = runner,
                    args = (batch, condition2, write_batch_async, num_writers))
                running_threads.append(thread)
                thread.start()
            else:
                thread = Thread(target = runner,
                    args = (batch, condition2, write_batch_sync, num_writers))
                running_threads.append(thread)
                thread.start()
        
        start = time.time()
        for cur_thread in running_threads:
            cur_thread.join()
        print("Time to write: " + str((time.time()-start)*1000) + "ms.")

        if batch_data['subscribe'] == "false":
            running_threads = []
            start = time.time()
            condition3 = Condition()
            condition3.cntr = 0
            for i in range(num_readers):
                if batch_data['multiread'] == "true":
                    thread = Thread(target = runner,
                        args = (batch, condition3, read_batch_multiread, num_readers))
                    running_threads.append(thread)
                    thread.start()
                elif batch_data['async_read'] == "true":
                    thread = Thread(target = runner,
                        args = (batch, condition3, read_batch_async, num_readers))
                    running_threads.append(thread)
                    thread.start()
                elif batch_data['async_read'] == "false":
                    thread = Thread(target = runner,
                        args = (batch, condition3, read_batch_sync, num_readers))
                    running_threads.append(thread)
                    thread.start()
            for cur_thread in running_threads:
                cur_thread.join()
            print("Time to read: " + str((time.time()-start)*1000) + "ms.")

if __name__ == "__main__":
    main()

"""
To Add:

* read_async not completed

* Timeouts need to be tweaked/added

* It currently assumes that the logs are already created.  To make it
easier to use, it should probably create the logs if requested.  I
suggest doing this on a flag for reasons I can explain later. (Come back later)

"""
