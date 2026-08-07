/*
 * From RFC1833
 */

const PMAP_PORT = 111;      /* portmapper port number */

struct mapping {
       unsigned int prog;
       unsigned int vers;
       unsigned int prot;
       unsigned int port;
};

struct call_args {
       unsigned int prog;
       unsigned int vers;
       unsigned int proc;
       opaque args<>;
};


program PMAP_PROGRAM {
	version PMAP_V2 {
        	void
		PMAP_NULL(void)         = 0;

		bool
            	PMAP_SET(mapping)       = 1;

            	bool
            	PMAP_UNSET(mapping)     = 2;

            	unsigned int
            	PMAP_GETPORT(mapping)   = 3;
	} = 2;
} = 100000;

