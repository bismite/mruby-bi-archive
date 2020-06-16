#include <mruby.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/string.h>

#include <mruby/msgpack.h>

#include <bi/texture.h>
#include <bi_core.h>

#include <string.h>
#include <stdlib.h>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

//
// Bi::Archive
//

typedef struct {
  void* buffer;
  // mrb_state *mrb;
  // mrb_value callback;
} mrb_archive;

void mrb_archive_free(mrb_state *mrb,void* p){
  mrb_archive *archive = p;
  mrb_free(mrb,archive->buffer);
}

static struct mrb_data_type const mrb_archive_data_type = { "Archive", mrb_archive_free };

static mrb_value mrb_archive_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value path;
  mrb_int secret;
  mrb_bool fallback = true;
  mrb_get_args(mrb, "Si|b", &path, &secret, &fallback );
  mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@path"), path );
  mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@secret"), mrb_fixnum_value(secret) );
  mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@available"), mrb_false_value() );
  mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@fallback"), mrb_bool_value(fallback) );
  mrb_archive* archive = mrb_malloc(mrb,sizeof(mrb_archive));
  DATA_PTR(self) = archive;
  DATA_TYPE(self) = &mrb_archive_data_type;
  return self;
}

static void read_from_rwops(mrb_state *mrb, mrb_value self, SDL_RWops *io)
{
  mrb_archive* archive;
  uint32_t table_length;
  Sint64 filesize = SDL_RWsize(io);

  archive = DATA_PTR(self);

  SDL_RWseek(io, 4, RW_SEEK_SET); // 32bit skip
  SDL_RWread(io,&table_length,4,1); // table size 32bit unsigned integer little endian

  char* buf = malloc(table_length);
  size_t table_loaded = SDL_RWread(io,buf,table_length,1);

  if(table_loaded!=1) {
    // XXX: raise!
    mrb_raise(mrb, E_RUNTIME_ERROR, "table load error.");
  }

  mrb_value raw_table_string = mrb_str_new_static(mrb,buf,table_length);
  mrb_value table = mrb_msgpack_unpack(mrb,raw_table_string);

  free(buf);

  mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@_table"), table );

  size_t contents_size = filesize - 4 - 4 - table_length;

  archive->buffer = mrb_malloc(mrb,contents_size);
  size_t buffer_loaded = SDL_RWread(io, archive->buffer, contents_size, 1);

  if(buffer_loaded!=1) {
    // XXX: raise!
    mrb_raise(mrb, E_RUNTIME_ERROR, "buffer load error.");
  }
}

static mrb_value mrb_archive_open(mrb_state *mrb, mrb_value self)
{
  mrb_value path = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb,"@path") );
  SDL_RWops *io = SDL_RWFromFile(mrb_string_value_cstr(mrb,&path),"rb");
  read_from_rwops(mrb,self,io);
  SDL_RWclose(io);
  return self;
}

#ifdef EMSCRIPTEN

typedef struct {
  mrb_state *mrb;
  mrb_value archive;
} fetch_context;

static void onload(unsigned int handle, void* _context, void* data, unsigned int size)
{
  fetch_context* context = (fetch_context*)_context;
  mrb_state *mrb = context->mrb;
  mrb_value self = context->archive;

  SDL_RWops* io = SDL_RWFromConstMem(data,size);
  read_from_rwops(mrb,self,io);
  SDL_RWclose(io);

  mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@available"), mrb_true_value() );
  mrb_value callback = mrb_iv_get(mrb,self, mrb_intern_cstr(mrb,"@callback"));
  mrb_value argv[1] = { self };
  if( mrb_symbol_p(callback) ){
    mrb_funcall_argv(mrb,self,mrb_symbol(callback),1,argv);
  }else if( mrb_type(callback) == MRB_TT_PROC ) {
    mrb_yield_argv(mrb,callback,1,argv);
  }

  free(context);
}

static void onprogress(unsigned int handle, void *_context, int loaded, int total)
{
  fetch_context* context = (fetch_context*)_context;
  mrb_state *mrb = context->mrb;
  mrb_value self = context->archive;

  mrb_value callback = mrb_iv_get(mrb,self,mrb_intern_cstr(mrb,"@on_progress"));
  mrb_value argv[3] = {
    self,
    mrb_fixnum_value(loaded),
    mrb_fixnum_value(total)
  };
  if( mrb_symbol_p(callback) ){
    mrb_funcall_argv(mrb,self,mrb_symbol(callback),3,argv);
  }else if( mrb_type(callback) == MRB_TT_PROC ) {
    mrb_yield_argv(mrb,callback,3,argv);
  }
}

static void onerror(unsigned int handle, void *_context, int http_status_code, const char* desc)
{
  fetch_context* context = (fetch_context*)_context;
  mrb_state *mrb = context->mrb;
  mrb_value self = context->archive;

  mrb_value callback = mrb_iv_get(mrb,self,mrb_intern_cstr(mrb,"@callback"));
  mrb_value argv[2] = {
    self,
    mrb_fixnum_value(http_status_code),
  };
  if( mrb_symbol_p(callback) ){
    mrb_funcall_argv(mrb,self,mrb_symbol(callback),2,argv);
  }else if( mrb_type(callback) == MRB_TT_PROC ) {
    mrb_yield_argv(mrb,callback,2,argv);
  }

  free(context);
}

static mrb_value mrb_archive_download(mrb_state *mrb, mrb_value self)
{
  mrb_value callback;
  mrb_value path;
  fetch_context *context;
  mrb_get_args(mrb,"o",&callback);

  mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@callback"), callback );
  context = malloc(sizeof(fetch_context));
  context->mrb = mrb;
  context->archive = self;

  path = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb,"@path") );

  emscripten_async_wget2_data(mrb_string_value_cstr(mrb,&path),"GET","",context,TRUE,onload,onerror,onprogress);

  return self;
}

#endif


static mrb_value mrb_archive_texture(mrb_state *mrb, mrb_value self)
{
  mrb_int start,length;
  mrb_bool antialias;
  mrb_archive* archive;
  mrb_get_args(mrb, "iib", &start,&length,&antialias);
  archive = DATA_PTR(self);
  return create_bi_texture_from_buffer( mrb, archive->buffer+start, length, antialias );
}

static mrb_value mrb_archive_texture_decrypt(mrb_state *mrb, mrb_value self)
{
  mrb_int secret,start,length;
  mrb_bool antialias;
  mrb_archive* archive;
  mrb_get_args(mrb, "iib", &start,&length,&antialias);
  archive = DATA_PTR(self);
  secret = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_cstr(mrb,"@secret")));
  uint8_t *buf = malloc(length);
  uint8_t *p = archive->buffer+start;
  for(int i=0;i<length;i++){
    buf[i] = p[i] ^ secret;
  }

  BiTexture texture;
  bi_create_texture( buf, length, &texture, antialias );

  mrb_value result = create_bi_texture( mrb, &texture );

  free(buf);

  return result;
}

static mrb_value mrb_archive_read_decrypt(mrb_state *mrb, mrb_value self)
{
  mrb_int secret,start,length;
  mrb_archive* archive;
  mrb_get_args(mrb, "ii", &start,&length);
  archive = DATA_PTR(self);
  secret = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_cstr(mrb,"@secret")));
  uint8_t *buf = malloc(length);
  uint8_t *p = archive->buffer+start;
  for(int i=0;i<length;i++){
    buf[i] = p[i] ^ secret;
  }

  mrb_value result = mrb_str_new(mrb, buf, length);

  free(buf);

  return result;
}

//
//

void mrb_mruby_bi_archive_gem_init(mrb_state* mrb)
{
  struct RClass *bi;
  bi = mrb_define_class(mrb, "Bi", mrb->object_class);
  MRB_SET_INSTANCE_TT(bi, MRB_TT_DATA);


  struct RClass *archive;

  archive = mrb_define_class_under(mrb, bi, "Archive", mrb->object_class);
  MRB_SET_INSTANCE_TT(archive, MRB_TT_DATA);

  mrb_define_method(mrb, archive, "initialize", mrb_archive_initialize, MRB_ARGS_ANY() ); // path,secret,(fallback)

  mrb_define_method(mrb, archive, "_open", mrb_archive_open, MRB_ARGS_NONE());

#ifdef EMSCRIPTEN
  mrb_define_method(mrb, archive, "_download", mrb_archive_download, MRB_ARGS_REQ(1)); // callback
#endif

  mrb_define_method(mrb, archive, "_texture", mrb_archive_texture, MRB_ARGS_REQ(3)); // start, size, antialias
  mrb_define_method(mrb, archive, "_texture_decrypt", mrb_archive_texture_decrypt, MRB_ARGS_REQ(3)); // start, size, antialias
  mrb_define_method(mrb, archive, "_read_decrypt", mrb_archive_read_decrypt, MRB_ARGS_REQ(2)); // start, size
}

void mrb_mruby_bi_archive_gem_final(mrb_state* mrb)
{
}
