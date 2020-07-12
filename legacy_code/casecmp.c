static int casecmp(s1, s2)
/* case-insensitive compare 2 strings */
/* return 0 if equal   */
/*        1 if s1 > s2 */
/*       -1 if s1 > s2 */
char s1[];
char s2[];
{
  int i, val, done;
  char c1, c2;

  i = 0;
  done = 0;
  while (done == 0) {
    c1 = tolower(s1[i]);
    c2 = tolower(s2[i]);
    if (c1 > c2) {
      val = 1;
      done = 1;
    } else {
      if (c1 < c2) {
        val = -1;
        done = 1;
      } else {
        if (c1 == '\0') {
          val = 0;
          done = 1;
        } else {
          i = i + 1;
        };
      };
    };
  };
  return(val);
}

static int stringcmp(s1, s2)
/* case sensitive compare 2 strings */
/* return 0 if equal   */
/*        1 if s1 > s2 */
/*       -1 if s1 > s2 */
char s1[];
char s2[];
{
  int i, val, done;

  i = 0;
  done = 0;
  while (done == 0) {
    if (s1[i] > s2[i]) {
      val = 1;
      done = 1;
    } else {
      if (s1[i] < s2[i]) {
        val = -1;
        done = 1;
      } else {
        if (s1[i] == '\0') {
          val = 0;
          done = 1;
        } else {
          i = i + 1;
        };
      };
    };
  };
  return(val);
}

