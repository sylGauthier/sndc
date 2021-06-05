extern const struct Module osc;
extern const struct Module mix;
extern const struct Module print;
extern const struct Module env;
extern const struct Module noise;
extern const struct Module filter;

const struct Module* modules[] = {
    &osc,
    &mix,
    &print,
    &env,
    &noise,
    &filter,
    NULL
};
