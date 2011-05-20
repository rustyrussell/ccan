
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ttxml.h"


XmlNode* xml_new(char * name, char * attrib)
{
	XmlNode * ret = malloc(sizeof(XmlNode));
	if(!ret)return NULL;

	ret->attrib = NULL;
	ret->nattrib = 0;
	ret->child = ret->next = NULL;

	ret->name = name;
	return ret;
}


void xml_free(XmlNode *target)
{
	int i;
	for(i=0; i<target->nattrib*2; i++)
		if(target->attrib[i])
			free(target->attrib[i]);

	if(target->attrib)free(target->attrib);
	if(target->child)xml_free(target->child);
	if(target->next)xml_free(target->next);
	free(target->name);
	free(target);
}

#define XML_LETTER	1
#define XML_NUMBER	2
#define XML_SPACE	4
#define XML_SLASH	8
#define XML_OPEN	16
#define XML_EQUALS	32
#define XML_CLOSE	64
#define XML_QUOTE	128
#define XML_OTHER	256

#define XML_ALL 0xFFFFFFFF

static int is_special(char item)
{
	if((item >= 'a' && item <= 'z') || (item >= 'A' && item <='Z'))
		return XML_LETTER;
	if( item >= '0' && item <='9' )
		return XML_NUMBER;
	if( item == 0x20 || item == '\t' ||	item == 0x0D || item == 0x0A )
		return XML_SPACE;
	if( item == '/' )
		return XML_SLASH;
	if( item == '<' )
		return XML_OPEN;
	if( item == '=' )
		return XML_EQUALS;
	if( item == '>' )
		return XML_CLOSE;
	if( item == '"' || item == '\'' )
		return XML_QUOTE;
	return 128;
}

struct XMLBUF
{
	FILE * fptr;
	char * buf;
	int len;
	int eof;
};


static void xml_consume(struct XMLBUF *xml, int offset)
{
	int size, request, received;
	
	size = xml->len - offset;

	if(!xml->len)
		return;

	if(size)
	{
//		printf("Size=%d, off=%d, len=%d\n", size, offset, xml->len);
		memmove(xml->buf, xml->buf + offset, size);
	}

	if(xml->eof)
	{
		xml->len = size;
		xml->buf[size]=0;
		return;
	}

	request = xml->len - size;
	received = fread(xml->buf + size, 1, request, xml->fptr);
	if( received == request )
		return;

	xml->len = size + received;
	xml->eof = 1;
	xml->buf[xml->len] = 0;
	return;
}


static void xml_skip( struct XMLBUF *xml, int mask )
{
	int offset = 0;
	if(!xml->len)return;
	while( is_special(xml->buf[offset]) & mask )
	{
		offset ++;
		if(offset == xml->len)
		{
			xml_consume(xml, offset);
			offset = 0;
			if(!xml->len)
				return;
		}
	}
	xml_consume(xml, offset);
}

static char quotechar = 0;
static int test_quote(const char x)
{
	static int escaped=0;
	if( escaped || '\\' == x )
	{
		escaped = !escaped;
		return 1;
	}
	if( x != quotechar )
		return 1;
	return 0;
}

static int feed_mask = 0;
static int test_mask(const char x)
{
	return !(is_special(x) & feed_mask);
}

static char* xml_feed( struct XMLBUF *xml, int (*test)(char) )
{
	int offset = 0;
	char *ret = NULL;
	int size = 0;

	while( test(xml->buf[offset]) )
	{
		offset++;
		if(offset == xml->len)
		{
			ret = realloc(ret, size+offset+1);
			memcpy(ret+size, xml->buf, offset);
			size += offset;
			ret[size]=0;
			xml_consume(xml, offset);
			offset = 0;
			if(!xml->len)return ret;
		}
	}

	if(offset)
	{
		ret = realloc(ret, size+offset+1);
		memcpy(ret+size, xml->buf, offset);
		size += offset;
		ret[size]=0;
		xml_consume(xml, offset);
	}
	return ret;
}

static void xml_read_attr(struct XMLBUF *xml, XmlNode *node)
{
	int n=0;

	// how does this tag finish?
	while(xml->len)
	{
		if( is_special(xml->buf[0]) & (XML_CLOSE | XML_SLASH) )
			return;

		n = ++node->nattrib;
		node->attrib = realloc(node->attrib, n * 2 * sizeof(char*) );
		node->attrib[--n*2+1] = 0;
		
		feed_mask = XML_EQUALS | XML_SPACE | XML_CLOSE | XML_SLASH;
		node->attrib[n*2] = xml_feed(xml, test_mask );
		if( xml->buf[0] == '=' )
		{
			if( is_special(xml->buf[1]) & XML_QUOTE )
			{
				quotechar = xml->buf[1];
				xml_consume(xml, 2);
				node->attrib[n*2+1] = xml_feed(xml, test_quote);
				xml_consume(xml, 1);
			}
			else
			{
				feed_mask = XML_SPACE | XML_CLOSE | XML_SLASH;
				xml_consume(xml, 1);
				node->attrib[n*2+1] = xml_feed(xml, test_mask);
			}
		}
		xml_skip(xml, XML_SPACE);
	}
}

static XmlNode* xml_parse(struct XMLBUF *xml)
{
	int offset;
	int toff;
	char *tmp;
	XmlNode **this, *ret = NULL;
	
	this = &ret;

	xml_skip(xml, XML_SPACE);	// skip whitespace
	offset=0;
	while(xml->len)
	{
		switch(is_special(xml->buf[offset]))
		{
			case XML_OPEN:
				xml_consume(xml, 1);
				if(xml->buf[offset] == '/')
					return ret;		// parents close tag
				// read the tag name
				feed_mask = XML_SPACE | XML_SLASH | XML_CLOSE;
				*this = xml_new( xml_feed(xml, test_mask), NULL );
				xml_skip(xml, XML_SPACE);	// skip any whitespace

				xml_read_attr(xml, *this);	// read attributes

				// how does this tag finish?
				switch(is_special(xml->buf[0]))
				{
					case XML_CLOSE:		// child-nodes ahead
						xml_consume(xml, 1);
						(*this)->child = xml_parse(xml);
						xml_skip(xml, XML_ALL ^ XML_CLOSE);
						xml_consume(xml, 1);
						break;
					case XML_SLASH:		// self closing tag
						xml_consume(xml, 2);
						break;
				}
				break;

			default:	// text node
				*this = xml_new(0, 0);
				xml_skip(xml, XML_SPACE);	// skip any whitespace
				feed_mask = XML_OPEN;
				(*this)->nattrib=1;
				(*this)->attrib = malloc(sizeof(char*)*2);
				tmp = (*this)->attrib[0] = xml_feed(xml, test_mask);
				toff = strlen(tmp)-1;
				while( ( is_special(tmp[toff]) & XML_SPACE ) )
				{
					tmp[toff] = 0;
					toff --;
				}

				(*this)->attrib[1] = NULL;
				break;
		}
		this = &(*this)->next; 
		xml_skip(xml, XML_SPACE);	// skip whitespace
	}	

	return ret;
}



#define BUF 3264
XmlNode* xml_load(const char * filename)
{
	struct XMLBUF xml;
	XmlNode *ret = NULL;

	xml.eof = 0;
	xml.fptr = fopen(filename, "rb");
	if(!xml.fptr)
	{
		printf("Opening file failed\n");
		return NULL;
	}

	xml.buf = malloc(BUF);
	if(!xml.buf)
		goto xml_load_fail_malloc_buf;
	
	xml.len = fread(xml.buf, 1, BUF, xml.fptr);
	if(xml.len < BUF)
		xml.eof = 1;

	ret = xml_parse(&xml);

	free(xml.buf);
xml_load_fail_malloc_buf:
	fclose(xml.fptr);
	return ret;
}
#undef BUF

XmlNode * xml_find(XmlNode *xml, const char *name)
{
	XmlNode * ret;
	if(xml->name)if(!strcmp(xml->name, name))return xml;
	if(xml->child)
	{
		ret = xml_find(xml->child, name);
		if(ret)return ret;
	}
	if(xml->next)
	{
		ret = xml_find(xml->next, name);
		if(ret)return ret;
	}
	return NULL;
}


char* xml_attr(XmlNode *x, const char *name)
{
	int i;
	for(i=0; i<x->nattrib; i++)
		if(x->attrib[i*2])
			if(!strcmp(x->attrib[i*2], name))
				return x->attrib[i*2+1];
	return 0;
}


#ifdef TEST
void xp(XmlNode *x, int level, int max)
{
	int i;
	char text[] = "text";
	char *name = text;
	if(level > max)return;
	if(!x)return;
	if(x->name)name = x->name;
	for(i=0; i<level; i++)printf("    ");
	printf("%s:", name);
	if(x->name)
	for(i=0; i<x->nattrib; i++)
		printf("%s=\"%s\",", x->attrib[i*2], x->attrib[i*2+1]);
	printf("\n");
	if(x->child)xp(x->child, level+1, max);
	if(x->next)xp(x->next, level, max);
}


int main(int argc, char *argv[])
{
	XmlNode *x;

	if(!argv[1])
	{
		printf("USAGE: %s name\n\t reads name where name is an XML file.\n",
				argv[0]);
		return 1;
	}

	printf("Loading file \"%s\"\n", argv[1]);

	x = xml_load(argv[1]);

	if(!x)
	{
		printf("Failed to load.\n");
		return 2;
	}

	xp(x, 1, 6);
	xml_free(x);
	printf("Happily free.\n");
	return 0;
}
#endif

