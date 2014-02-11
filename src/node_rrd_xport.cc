/*
    RRDtool (http://oss.oetiker.ch/rrdtool/) bindings module for node (http://nodejs.org)
    
    Copyright (c), 2012 Thierry Passeron

    The MIT License

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/
#include <cstdio>

#include "node_rrd.h"

namespace node_rrd {

namespace {

class Infos: public AsyncInfos {
public:
    unsigned long step;
    time_t start;
    time_t end;
    int argc;
    char **argv;
    char **legend_v;
    unsigned long col_cnt;
    rrd_value_t *data;

    ~Infos();
};

Infos::~Infos() {
    for (int i = 0; i < argc; i++) free(argv[i]);
    free(argv);
    for (unsigned long i = 0; i < col_cnt; i++) free(legend_v[i]);
    free(legend_v);
    free(data);
}

}

static void async_worker(uv_work_t *req);
static void async_after(uv_work_t *req);

Handle<Value> xport(const Arguments &args) { /* rrd.xport(Array spec, Function callback); */
    HandleScope scope;

    CHECK_FUN_ARG(1)

    // Create info baton
    CREATE_ASYNC_BATON(Infos, info)

    // Get spec array
    SET_ARGC_ARGV_ARG(0, info->argc, info->argv)

    // Get callback
    SET_PERSFUN_ARG(1, info->callback)

    uv_queue_work(uv_default_loop(), &info->request, async_worker, (uv_after_work_cb)async_after);

    return Undefined();
}

static void async_worker(uv_work_t *req) {
    Infos * info = static_cast<Infos*>(req->data);
    int xsize;

    info->status = rrd_xport(
    	info->argc,
    	info->argv,
    	&xsize,
        &info->start,
        &info->end,
        &info->step,
        &info->col_cnt,
        &info->legend_v,
        &info->data
    );
}

Handle<Object> create_object(Infos * info, rrd_value_t *data);
Handle<Array> current_data_to_array(unsigned long col_cnt, rrd_value_t *data);

static void async_after(uv_work_t *req) {
    HandleScope scope;

    Infos * info = static_cast<Infos*>(req->data);
    if (info->status == 0) {
        rrd_value_t *datai;
        datai = info->data;
    	Handle<Object> result = create_object(info, datai);
        Handle<Value> argv[] = { Null(), result };
        info->callback->Call(Context::GetCurrent()->Global(), 2, argv);
    } else {
        Handle<Value> res[] = { Number::New(info->status), String::New(rrd_get_error()) };
        info->callback->Call(Context::GetCurrent()->Global(), 2, res);
    }
    rrd_clear_error();
    delete(info);
}

Handle<Object> create_object(Infos * info, rrd_value_t *data) {
    HandleScope scope;

    Handle<ObjectTemplate> obj = ObjectTemplate::New();
    Handle<Object> result = obj->NewInstance();

    Handle<Object> exportedData = obj->NewInstance();
    rrd_value_t *datai;
    long ti;

    datai = info->data;
    int numRows = 0;
    for (ti = (info->start + info->step); ti < info->end; ti += info->step) {
    	numRows += 1;
    	exportedData->Set(Number::New(ti), current_data_to_array(info->col_cnt, datai));
        datai += info->col_cnt;
    }
    result->Set(String::New("data"), exportedData);

    Handle<Object> meta = obj->NewInstance();
    meta->Set(String::New("start"), Number::New(info->start));
    meta->Set(String::New("end"), Number::New(info->end));
    meta->Set(String::New("step"), Number::New(info->step));
    meta->Set(String::New("columns"), Number::New(info->col_cnt));
    meta->Set(String::New("rows"), Number::New(numRows));
    Handle<Array> legend = Array::New(info->col_cnt);
    for (unsigned long  i = 0; i < info->col_cnt; i++) {
    	legend->Set(i, String::New(info->legend_v[i]));
    }
    meta->Set(String::New("legend"), legend);

    result->Set(String::New("meta"), meta);

    return scope.Close(result);
}

Handle<Array> current_data_to_array(unsigned long col_cnt, rrd_value_t *data) {
    HandleScope scope;

    Handle<Array> result = Array::New(col_cnt);

    unsigned long ii;
    rrd_value_t *datai;

    datai = data;
    for (ii = 0; ii < col_cnt; ii++) {
    	result->Set(ii, Number::New(datai[ii]));
    }

    return scope.Close(result);
}
}
