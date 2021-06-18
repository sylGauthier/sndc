extern const struct Module osc;
extern const struct Module mix;
extern const struct Module print;
extern const struct Module env;
extern const struct Module noise;
extern const struct Module filter;
extern const struct Module add;
extern const struct Module mul;
extern const struct Module func;
extern const struct Module drumbox;

const struct Module* modules[] = {
    &osc,
    &mix,
    &print,
    &env,
    &noise,
    &filter,
    &add,
    &mul,
    &func,
    &drumbox,
    NULL
};
