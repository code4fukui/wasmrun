int sp;
int s0;
int s1;
int s2;

void push(int v) {
  if (sp == 0) {
    s0 = v;
  } else if (sp == 1) {
    s1 = v;
  } else {
    s2 = v;
  }
  sp = sp + 1;
}

int pop() {
  sp = sp - 1;
  if (sp == 0) {
    return s0;
  } else if (sp == 1) {
    return s1;
  }
  return s2;
}

int gstack(int a, int b, int c) {
  push(a);
  push(b);
  push(c);
  return pop() * pop() + pop();
}

int main() {
  return gstack(2, 3, 4);
}
