
# include <sym/utilities.h>
# include <assert.h>

#define REMOTE_URL "127.0.0.1:40001"

int main( int argc, char **argv)
{
    sym::util::Array<char> array(64);
    
    for( int i =0; i < 16; ++i) array[i] = i;

    for( int i =0; i < 16; ++i) assert(array[i] == i);
    return 0;
}

