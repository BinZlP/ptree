#include <stdlib.h>
#include <stdio.h>
#include "prinfo.h"

int main(void){
  int ret;
  struct prinfo* buf;
  int nr=50;
  buf = (struct prinfo*)malloc(sizeof(struct prinfo)*nr);
  ret = syscall(548, buf, &nr);

  struct prinfo p;
  for(int i=0;i<nr;i++){
    p = buf[i];
    printf("%s,%d,%lld,%d,%d,%d,%lld\n", p.comm, p.pid, p.state,
      p.parent_pid, p.first_child_pid, p.next_sibling_pid, p.uid);
  }

  printf("%d\n", ret);
  return 0;
}
