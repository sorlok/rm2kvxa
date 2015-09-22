#include <ruby.h>

#include <vector>
#include <string>
#include <iostream>

//NOTE: Probably not needed.
//#include <sigc++/signal.h>

//NOTE: A lot of this code comes from the mkxp project.
//      Consider this particular file a derivative work, 
//      licensed under the same terms:
//      https://github.com/Ancurio/mkxp


// 2.1 has added a new field (flags) to rb_data_type_t
// This likely does not affect us, as we are using 1.9.3
#define DEF_TYPE_FLAGS

#define DEF_TYPE_CUSTOMNAME_AND_FREE(Klass, Name, Free) \
	rb_data_type_t Klass##Type = { \
		Name, { 0, Free, 0, { 0, 0 } }, 0, 0, DEF_TYPE_FLAGS \
	}

#define DEF_TYPE_CUSTOMFREE(Klass, Free) \
	DEF_TYPE_CUSTOMNAME_AND_FREE(Klass, #Klass, Free)

#define DEF_TYPE_CUSTOMNAME(Klass, Name) \
	DEF_TYPE_CUSTOMNAME_AND_FREE(Klass, Name, freeInstance<Klass>)

#define DEF_TYPE(Klass) DEF_TYPE_CUSTOMNAME(Klass, #Klass)


template<class C>
static void freeInstance(void *inst)
{
	delete static_cast<C*>(inst);
}


void
writeInt32(char **dataP, int32_t value)
{
	memcpy(*dataP, &value, 4);
	*dataP += 4;
}

int32_t
readInt32(const char **dataP)
{
	int32_t result;

	memcpy(&result, *dataP, 4);
	*dataP += 4;

	return result;
}


template<typename C>
inline const C *dataPtr(const std::vector<C> &v)
{
	return v.empty() ? (C*)0 : &v[0];
}

template<typename C>
inline C *dataPtr(std::vector<C> &v)
{
	return v.empty() ? (C*)0 : &v[0];
}


//The actual class
struct Serializable
{
  virtual int serialSize() const = 0;
  virtual void serialize(char *buffer) const = 0;
};


class Table : public Serializable
{
public:
	Table(int x, int y = 1, int z = 1)     : xs(x), ys(y), zs(z),
      data(x*y*z)
     {}

	Table(const Table &other)     : xs(other.xs), ys(other.ys), zs(other.zs),
      data(other.data)
     {}

	virtual ~Table() {}

	int xSize() const { return xs; }
	int ySize() const { return ys; }
	int zSize() const { return zs; }

	int16_t get(int x, int y = 0, int z = 0) const
{
	return data[xs*ys*z + xs*y + x];
}

	void set(int16_t value, int x, int y = 0, int z = 0)
{
	if (x < 0 || x >= xs
	||  y < 0 || y >= ys
	||  z < 0 || z >= zs)
	{
		return;
	}

	data[xs*ys*z + xs*y + x] = value;

	//modified();
}

	void resize(int x, int y, int z)
{
	if (x == xs && y == ys && z == zs)
		return;

	std::vector<int16_t> newData(x*y*z);

	for (int k = 0; k < std::min(z, zs); ++k)
		for (int j = 0; j < std::min(y, ys); ++j)
			for (int i = 0; i < std::min(x, xs); ++i)
				newData[x*y*k + x*j + i] = at(i, j, k);

	data.swap(newData);

	xs = x;
	ys = y;
	zs = z;

	return;
}


	void resize(int x, int y)
{
	resize(x, y, zs);
}


	void resize(int x)
{
	resize(x, ys, zs);
}


	int serialSize() const
{
	/* header + data */
	return 20 + (xs * ys * zs) * 2;
}

	void serialize(char *buffer) const
{
	/* Table dimensions: we don't care
	 * about them but RMXP needs them */
	int dim = 1;
	int size = xs * ys * zs;

	if (ys > 1)
		dim = 2;

	if (zs > 1)
		dim = 3;

	writeInt32(&buffer, dim);
	writeInt32(&buffer, xs);
	writeInt32(&buffer, ys);
	writeInt32(&buffer, zs);
	writeInt32(&buffer, size);

	memcpy(buffer, dataPtr(data), sizeof(int16_t)*size);
}

	static Table *deserialize(const char *data, int len)
{
	if (len < 20) {
                std::cout <<"Marshal: Table: bad file format\n";
		throw "Marshal: Table: bad file format";
        }

	readInt32(&data);
	int x = readInt32(&data);
	int y = readInt32(&data);
	int z = readInt32(&data);
	int size = readInt32(&data);

	if (size != x*y*z) {
                std::cout <<"Marshal: Table: bad file format\n";
		throw "Marshal: Table: bad file format";
        }

	if (len != 20 + x*y*z*2) {
                std::cout <<"Marshal: Table: bad file format\n";
		throw "Marshal: Table: bad file format";
        }

	Table *t = new Table(x, y, z);
	memcpy(dataPtr(t->data), data, sizeof(int16_t)*size);

	return t;
}

	/* <internal */
	inline int16_t &at(int x, int y = 0, int z = 0)
	{
		return data[xs*ys*z + xs*y + x];
	}

	inline const int16_t &at(int x, int y = 0, int z = 0) const
	{
		return data[xs*ys*z + xs*y + x];
	}

	//sigc::signal<void> modified;

private:
	int xs, ys, zs;
	std::vector<int16_t> data;
};


//Other stuff

typedef VALUE (*RubyMethod)(int argc, VALUE *argv, VALUE self);

static inline void
rb_define_class_method(VALUE klass, const char *name, RubyMethod func)
{
	rb_define_singleton_method(klass, name, RUBY_METHOD_FUNC(func), -1);
}

void _rb_define_method(VALUE klass, const char *name, RubyMethod func)
{
	rb_define_method(klass, name, RUBY_METHOD_FUNC(func), -1);
}

struct Exception
{
	enum Type
	{
		RGSSError,
		NoFileError,
		IOError,

		/* Already defined by ruby */
		TypeError,
		ArgumentError,

		/* New types introduced in mkxp */
		PHYSFSError,
		SDLError,
		MKXPError
	};

	Type type;
	std::string msg;

	Exception(Type type, const char *format, ...)
	    : type(type)
	{
		va_list ap;
		va_start(ap, format);

		msg.resize(512);
		vsnprintf(&msg[0], msg.size(), format, ap);

		va_end(ap);
	}
};

/*struct RbData
{
	VALUE exc[RbExceptionsMax];

	// Input module (RGSS3) 
	VALUE buttoncodeHash;

	RbData();
	~RbData();
};

RbData *getRbData()
{
	return static_cast<RbData*>(shState->bindingData()); //TODO: What is this?
}

void raiseRbExc(const Exception &exc)
{
	RbData *data = getRbData();
	VALUE excClass = data->exc[excToRbExc[exc.type]];

	rb_raise(excClass, "%s", exc.msg.c_str());
}*/

void
rb_float_arg(VALUE arg, double *out, int argPos = 0)
{
	switch (rb_type(arg))
	{
	case RUBY_T_FLOAT :
		*out = RFLOAT_VALUE(arg);
		break;

	case RUBY_T_FIXNUM :
		*out = FIX2INT(arg);
		break;

	default:
		rb_raise(rb_eTypeError, "Argument %d: Expected float", argPos);
	}
}

void
rb_int_arg(VALUE arg, int *out, int argPos = 0)
{
	switch (rb_type(arg))
	{
	case RUBY_T_FLOAT :
		// FIXME check int range?
		*out = NUM2LONG(arg);
		break;

	case RUBY_T_FIXNUM :
		*out = FIX2INT(arg);
		break;

	default:
		rb_raise(rb_eTypeError, "Argument %d: Expected fixnum", argPos);
	}
}

void
rb_bool_arg(VALUE arg, bool *out, int argPos = 0)
{
	switch (rb_type(arg))
	{
	case RUBY_T_TRUE :
		*out = true;
		break;

	case RUBY_T_FALSE :
	case RUBY_T_NIL :
		*out = false;
		break;

	default:
		rb_raise(rb_eTypeError, "Argument %d: Expected bool", argPos);
	}
}


int
rb_get_args(int argc, VALUE *argv, const char *format, ...) {
	char c;
	VALUE *arg = argv;
	va_list ap;
	bool opt = false;
	int argI = 0;

	va_start(ap, format);

	while ((c = *format++))
	{
		switch (c)
		{
	    case '|' :
			break;
	    default:
		// FIXME print num of needed args vs provided
			if (argc <= argI && !opt)
				rb_raise(rb_eArgError, "wrong number of arguments");

			break;
	    }

		if (argI >= argc)
			break;

		switch (c)
		{
		case 'o' :
		{
			if (argI >= argc)
				break;

			VALUE *obj = va_arg(ap, VALUE*);

			*obj = *arg++;
			++argI;

			break;
		}

		case 'S' :
		{
			if (argI >= argc)
				break;

			VALUE *str = va_arg(ap, VALUE*);
			VALUE tmp = *arg;

			if (!RB_TYPE_P(tmp, RUBY_T_STRING))
				rb_raise(rb_eTypeError, "Argument %d: Expected string", argI);

			*str = tmp;
			++argI;

			break;
		}

		case 's' :
		{
			if (argI >= argc)
				break;

			const char **s = va_arg(ap, const char**);
			int *len = va_arg(ap, int*);

			VALUE tmp = *arg;

			if (!RB_TYPE_P(tmp, RUBY_T_STRING))
				rb_raise(rb_eTypeError, "Argument %d: Expected string", argI);

			*s = RSTRING_PTR(tmp);
			*len = RSTRING_LEN(tmp);
			++argI;

			break;
		}

		case 'z' :
		{
			if (argI >= argc)
				break;

			const char **s = va_arg(ap, const char**);

			VALUE tmp = *arg++;

			if (!RB_TYPE_P(tmp, RUBY_T_STRING))
				rb_raise(rb_eTypeError, "Argument %d: Expected string", argI);

			*s = RSTRING_PTR(tmp);
			++argI;

			break;
		}

		case 'f' :
		{
			if (argI >= argc)
				break;

			double *f = va_arg(ap, double*);
			VALUE fVal = *arg++;

			rb_float_arg(fVal, f, argI);

			++argI;
			break;
		}

		case 'i' :
		{
			if (argI >= argc)
				break;

			int *i = va_arg(ap, int*);
			VALUE iVal = *arg++;

			rb_int_arg(iVal, i, argI);

			++argI;
			break;
		}

		case 'b' :
		{
			if (argI >= argc)
				break;

			bool *b = va_arg(ap, bool*);
			VALUE bVal = *arg++;

			rb_bool_arg(bVal, b, argI);

			++argI;
			break;
		}

		case 'n' :
		{
			if (argI >= argc)
				break;

			ID *sym = va_arg(ap, ID*);

			VALUE symVal = *arg++;

			if (!SYMBOL_P(symVal))
				rb_raise(rb_eTypeError, "Argument %d: Expected symbol", argI);

			*sym = SYM2ID(symVal);
			++argI;

			break;
		}

		case '|' :
			opt = true;
			break;

		default:
			rb_raise(rb_eFatal, "invalid argument specifier %c", c);
		}
	}
	va_end(ap);

	return argI;
}

#define RB_ARG_END

#define GUARD_EXC(exp) exp
//{ try { exp } catch (const Exception &exc) { raiseRbExc(exc); } }


static inline void
setPrivateData(VALUE self, void *p)
{
	RTYPEDDATA_DATA(self) = p;
}


template<class C>
static inline VALUE
objectLoad(int argc, VALUE *argv, VALUE self)
{
  const char *data;
  int dataLen;
  rb_get_args(argc, argv, "s", &data, &dataLen RB_ARG_END);

  VALUE obj = rb_obj_alloc(self);

  C *c = 0;

  GUARD_EXC( c = C::deserialize(data, dataLen); );

  setPrivateData(obj, c);

  return obj;
}

#define RB_METHOD(name) \
  static VALUE name(int argc, VALUE *argv, VALUE self)

#define RB_UNUSED_PARAM \
  { (void) argc; (void) argv; (void) self; }

#define MARSH_LOAD_FUN(Typ) \
  RB_METHOD(Typ##Load) \
  { \
    return objectLoad<Typ>(argc, argv, self); \
  }

/*#define INITCOPY_FUN(Klass) \
  RB_METHOD(Klass##InitializeCopy) \
  { \
    VALUE origObj; \
    rb_get_args(argc, argv, "o", &origObj RB_ARG_END); \
    if (!OBJ_INIT_COPY(self, origObj)) // When would this fail??\
      return self; \
    Klass *orig = getPrivateData<Klass>(origObj); \
    Klass *k = 0; \
    GUARD_EXC( k = new Klass(*orig); ) \
    setPrivateData(self, k); \
    return self; \
  }*/


template<rb_data_type_t* rbType>
static VALUE classAllocate(VALUE klass) {
  return rb_data_typed_object_alloc(klass, 0, rbType);
}

template<class C>
C *deserialize(const char *data)
{
  return C::deserialize(data);
}

template<class C>
inline C *
getPrivateData(VALUE self)
{
	C *c = static_cast<C*>(RTYPEDDATA_DATA(self));

	return c;
}


template<class C>
static VALUE
serializableDump(int, VALUE *, VALUE self)
{
	Serializable *s = getPrivateData<C>(self);

	int dataSize = s->serialSize();

	VALUE data = rb_str_new(0, dataSize);

	GUARD_EXC( s->serialize(RSTRING_PTR(data)); );

	return data;
}

template<class C>
void
serializableBindingInit(VALUE klass)
{
  _rb_define_method(klass, "_dump", serializableDump<C>);
}

static int num2TableSize(VALUE v)
{
	int i = NUM2INT(v);
	return std::max(0, i);
}

static void parseArgsTableSizes(int argc, VALUE *argv, int *x, int *y, int *z)
{
	*y = *z = 1;

	switch (argc)
	{
	case 3:
		*z = num2TableSize(argv[2]);
		/* fall through */
	case 2:
		*y = num2TableSize(argv[1]);
		/* fall through */
	case 1:
		*x = num2TableSize(argv[0]);
		break;
	default:
               std::cout <<"parseArgsTableSizes error!\n";
               throw "Error!";
		//rb_error_arity(argc, 1, 3);
	}
}


DEF_TYPE(Table);

RB_METHOD(tableInitialize)
{
std::cout <<"TEMP -- TESTING\n";
	int x, y, z;

	parseArgsTableSizes(argc, argv, &x, &y, &z);

	Table *t = new Table(x, y, z);

	setPrivateData(self, t);

	return self;
}


//The actual bindings

RB_METHOD(tableGetAt)
{
	Table *t = getPrivateData<Table>(self);

	int x, y, z;
	x = y = z = 0;

	x = NUM2INT(argv[0]);
	if (argc > 1)
		y = NUM2INT(argv[1]);
	if (argc > 2)
		z = NUM2INT(argv[2]);

	if (argc > 3)
		rb_raise(rb_eArgError, "wrong number of arguments");

	if (x < 0 || x >= t->xSize()
	||  y < 0 || y >= t->ySize()
	||  z < 0 || z >= t->zSize())
	{
		return Qnil;
	}

	short result = t->get(x, y, z);

	return INT2FIX(result); /* short always fits in a Fixnum */
}

RB_METHOD(tableSetAt)
{
	Table *t = getPrivateData<Table>(self);

	int x, y, z, value;
	x = y = z = 0;

	if (argc < 2)
		rb_raise(rb_eArgError, "wrong number of arguments");

	switch (argc)
	{
	default:
	case 2 :
		x = NUM2INT(argv[0]);
		value = NUM2INT(argv[1]);

		break;
	case 3 :
		x = NUM2INT(argv[0]);
		y = NUM2INT(argv[1]);
		value = NUM2INT(argv[2]);

		break;
	case 4 :
		x = NUM2INT(argv[0]);
		y = NUM2INT(argv[1]);
		z = NUM2INT(argv[2]);
		value = NUM2INT(argv[3]);

		break;
	}

	t->set(value, x, y, z);

	return argv[argc - 1];
}

MARSH_LOAD_FUN(Table)
//INITCOPY_FUN(Table) //NOTE: We don't have this available in 1.9

extern "C" void Init_rmvx_small() {
  //Set up our bindings
  VALUE klass = rb_define_class("Table", rb_cObject);
  rb_define_alloc_func(klass, classAllocate<&TableType>);

  serializableBindingInit<Table>(klass);

  rb_define_class_method(klass, "_load", TableLoad);
  _rb_define_method(klass, "initialize", tableInitialize);
  //_rb_define_method(klass, "initialize_copy", TableInitializeCopy);

  _rb_define_method(klass, "[]", tableGetAt);
  _rb_define_method(klass, "[]=", tableSetAt);

  //Make sure there are no missing methods
  Table t(1);
}

