/*
 * $I$
 */

#include <iostream>
#include "wtext.h"
using namespace std;

// derived from code by Evan Teran
// http://stackoverflow.com/questions/311955/redirecting-cout-to-a-console-in-windows

class outbuf : public streambuf
{
  public:
    outbuf() {
	setp(0, 0);
    }

    virtual int_type overflow(int_type c = traits_type::eof()) {
	return fputc(c, stdout) == EOF ? traits_type::eof() : c;
    }
};


extern "C" void
RedirectOutputStreams(int init)
{
    static outbuf ob;
    static streambuf * sb_cout;
    static streambuf * sb_cerr;

    if (init) {
	// let cout/cerr use our custom streambuf
	sb_cout = cout.rdbuf(&ob);
	sb_cerr = cerr.rdbuf(&ob);
    } else {
	// make sure to restore the original so we don't get a crash on close!
	cout.rdbuf(sb_cout);
	cerr.rdbuf(sb_cerr);
    }
}
