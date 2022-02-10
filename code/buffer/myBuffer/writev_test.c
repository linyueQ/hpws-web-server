#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>

int main(int argc,char **argv) {
        int z; /* Return status code */
        static char buf1[] = "by writev(2)";/* Middle buffer */
        static char buf2[] =">;>;>;"; /* Last buffer */
        static char buf3[] = "<<<"; /* First buffer */
        static char buf4[] = "\n"; /* Newline at end */
        struct iovec iov[4]; /* Handles 4 buffers */

        iov[0].iov_base = buf3;
        iov[0].iov_len = strlen(buf3);
        iov[1].iov_base = buf1;
        iov[1].iov_len = strlen(buf1);
        iov[2].iov_base = buf2;
        iov[2].iov_len = strlen(buf2);
        iov[3].iov_base = buf4;
        iov[3].iov_len = strlen(buf4); //可以为0，不能为负
        
        int readPos_=0;
        printf("%d\n",readPos_-1>=0 ? readPos_-1:0);

        z = writev(1,&iov[0],4); /* scatter write 4
        buffers */
        if ( z == -1 )
                abort(); /* Failed */

        return 0;
}