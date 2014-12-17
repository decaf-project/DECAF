extern struct af_vnode vnode_raw;	/* vnode_raw.cpp */

int raw_freopen(AFFILE *af,FILE *f);
int raw_popen(AFFILE *af,const char *command,const char *type);
