extern const struct Module osc;
extern const struct Module mix;
extern const struct Module print;
extern const struct Module env;

const struct Module* modules[] = {
    &osc,
    &mix,
    &print,
    &env,
    NULL
};
