//Achtung - compiler flag setzen: -fshort-enums

#ifndef ledenums_h
#define ledenums_h

enum TimeUnit {
  milli,
  tensec,
  tenhour
};
 
enum Operator {
	op_equal,
	op_inequal,
	op_greater,
	op_greatequal,
	op_less,
	op_lessequal,
	op_not,
	op_and,
	op_or,
	op_add,
	op_sub,
	op_mult,
	op_div,
	op_mod,
	op_bitand,
	op_bitor
};

#endif
