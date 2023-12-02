#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct  State{
    unsigned short blockmap;
    unsigned char me:4;
    unsigned char enemy:4;
}State;

typedef enum Result {
    untouched = 0,
    win = 1,
    lost = 2,
    visited =3
}Result ;

unsigned char ResultMap[ 1 << (16 + 4 + 4 - 2) ];

const State invalid = {.blockmap= 0, .me = 0, .enemy= 0};

bool IsValid(State s) {
  return s.me != s.enemy;
}


unsigned short reverseBits(unsigned short x)
{
	unsigned short s = sizeof(x) * 8;
	unsigned short mask = 0xFFFF;
	while ((s >>= 1) > 0)
	{
		mask ^= mask << s;
		x = ((x >> s) & mask) | ((x << s) & ~mask);
	}
	return x;
}

int NewPos(int pos, int direction, int count) {
  assert(direction == 1 || direction == -1 || direction == 4 || direction == -4);
  assert(count == 1 || count == 2);
  int newpos = pos + direction * count;
  if (newpos < 0 || newpos > 0xF)
    return -1;
  if ((newpos >> 2) != (pos >> 2)  && (newpos & 0x3) != (pos & 0x3))
    return -1;
  return newpos;
}

State Advance(State s, int direction, int count) {

  const int offset = direction * count;
  int currpos = s.me;
  int newpos = NewPos(currpos, direction, count);
  if (newpos == -1)
    return invalid;
  
  do {
    currpos += direction;
    if (currpos  == s.enemy || s.blockmap & (1 << currpos))
      return invalid;
  } while (currpos != newpos);

  s.me = newpos;
  int futurePos = NewPos(newpos, direction, 1);
  if (futurePos == -1 || futurePos == s.enemy)
    return s;
  if (!(s.blockmap & (1 << futurePos))) {
   s.blockmap |= (1 << futurePos);
   return s;
  }
  if (count == 2)
    return s;
  for(;;) {
    int morefuture = NewPos(futurePos, direction, 1);
    if(morefuture  == -1 || morefuture  == s.enemy) {
      for (int cleanpos = s.me + direction; cleanpos != futurePos + direction; cleanpos += direction) {
        s.blockmap ^= (1 << cleanpos);
      }
      return s;
    }
    if (!(s.blockmap & (1 << morefuture))) {
        s.blockmap ^= (1 << futurePos);
        s.blockmap ^= (1 << morefuture);
        return s;
    }
    futurePos = morefuture;
  }

  // unreachable here;
}


State TurnOver(State s) {
  State turned;
  turned.blockmap = reverseBits(s.blockmap);
  turned.enemy = 0x0F - s.me;
  turned.me = 0x0F - s.enemy;
  return turned;
}

int CalcNextState(State s, State post_states[8]) {
  int result_idx = 0;
  int directions[4] = {-4,-1,1,4};
  for (int idx = 0; idx < 4; ++idx) {
    const int direction = directions[idx];
    State next = Advance(s, direction, 1);
    if (IsValid(next)) {
      post_states[result_idx++] = next;
      State next = Advance(s, direction, 2);
      if (IsValid(next)) {
         post_states[result_idx++] = next;
      }
    }
  }
  if (result_idx == 0) {
    post_states[result_idx++] = s;
  }
  return result_idx;
}

Result FetchResult(State s) {
  bool turned = false;
  if(s.me > s.enemy) {
    s = TurnOver(s);
    turned = 1;
  }
  int offset = (s.blockmap << 6) | (s.me << 2) | (s.enemy >>2);
  int shift = (s.enemy & 0x3) << 1;
  unsigned char mask = 0x3 << shift;
  Result r = (ResultMap[offset] & mask) >> shift;
  return turned ?  ((r & 0x1) << 1) | (r >> 1) : r;
}

void SaveResult(State s, Result result) {
  if(s.me > s.enemy) {
    s = TurnOver(s);
    result = ((result  & 0x1) << 1) | (result  >> 1);
  }
  int offset = (s.blockmap << 6) | (s.me << 2) | (s.enemy >>2);
  int shift = (s.enemy & 0x3) << 1;
  unsigned char mask = 0x3 << shift;
  ResultMap[offset] = (ResultMap[offset] & ~mask) | (result << shift);
}

Result search (State s, int depth) {
  if(s.me == 0xF) 
    return win;
  if(s.enemy == 0x0) 
    return lost;

  Result cached =  FetchResult(s);
  if(cached != untouched) {
    return cached;
  }

  SaveResult(s, visited);

  State post_states[8] = {};
  int next_states = CalcNextState(s, post_states);
  assert(0 != next_states);

  int existsLoop = 0;
  for (int idx = 0; idx < next_states; ++idx) {
    Result next_result = search(TurnOver(post_states[idx]), depth + 1);
    if(next_result == lost){
      SaveResult(s, win);
      return win;
    } else if (next_result == visited) {
      existsLoop = 1;
    }
  }
  if(!existsLoop) {
   SaveResult(s, lost);
   return lost;
  }
  /* SaveResult(s, untouched); */
  return visited;
}

int main() {
  State s;
  s.blockmap = 0x0660;
  s.me = 0;
  s.enemy = 0xF;
  Result  r = search(s, 0);
  printf ("result = %d\n", r);
  return 0;
}
