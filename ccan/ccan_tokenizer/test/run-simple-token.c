#include <ccan/ccan_tokenizer/read_cnumber.c>
#include <ccan/ccan_tokenizer/read_cstring.c>
#include <ccan/ccan_tokenizer/dict.c>
#include <ccan/ccan_tokenizer/ccan_tokenizer.c>
#include <ccan/ccan_tokenizer/queue.c>
#include <ccan/ccan_tokenizer/charflag.c>
#include <ccan/tap/tap.h>

#define item(num) (toks->first[num])
//sed 's/toks->array\.item\[\([^]]*\)\]/item(\1)/g'

tok_message_queue *MQ = NULL;

static const char *onechar_tokens = "!~#%^&*()=-+{}[]|;:,.<>/?";
static const char *twochar_tokens = "!=##%=^=&=&&*=-=--->+=++==|=||<=<<>=>>/=";
static const char *threechar_tokens = "<<=>>=...";
static const char *char_token = "'x'";
static const char *string_token = "\"string\"";
static const char *ident_tokens = "doh abc f_o _ba b$f";

static char *backslashify(const char *string)
{
	unsigned int i;
	char *ret = talloc_size(NULL, strlen(string)*3 + 1);
	for (i = 0; i < strlen(string); i++) {
		ret[i*3] = string[i];
		ret[i*3+1] = '\\';
		ret[i*3+2] = '\n';
	}
	ret[i*3] = '\0';
	return ret;
}

static char *spacify(const char *string, unsigned int num)
{
	unsigned int i;
	char *ret = talloc_size(NULL, strlen(string)*2 + 1);
	memset(ret, ' ', strlen(string)*2);

	for (i = 0; i < strlen(string); i += num)
		memcpy(&ret[i + i/num], string+i, num);
	ret[i + i/num] = '\0';
	return ret;
}

static struct token_list *test_tokens(const char *orig, unsigned int size)
{
	struct token_list *toks;
	char *string = talloc_strdup(NULL, orig);
	unsigned int i;

	toks = tokenize(string, string, strlen(string), MQ);
	ok1(token_list_sanity_check(toks, stdout));
	
	ok1(token_list_count(toks) == strlen(string)/size + 1);
	ok1(item(0).type == TOK_STARTLINE);
	for (i = 0; i < strlen(string)/size; i++) {
		ok1(item(i+1).type == TOK_OPERATOR);
		ok1(item(i+1).txt_size == size);
		ok1(strncmp(item(i+1).txt, string + i*size, size) == 0);
		ok1(item(i+1).orig_size == size);
		ok1(item(i+1).orig == string + i*size);
	}
	return toks;
}

static struct token_list *test_tokens_spaced(const char *orig,
					     unsigned int size)
{
	struct token_list *toks;
	char *string = spacify(orig, size);
	unsigned int i;

	toks = tokenize(string, string, strlen(string), MQ);
	ok1(token_list_sanity_check(toks, stdout));
	
	ok1(token_list_count(toks) == strlen(orig)/size*2 + 1);
	ok1(item(0).type == TOK_STARTLINE);
	for (i = 0; i < strlen(orig)/size; i++) {
		ok1(item(i*2+1).type == TOK_OPERATOR);
		ok1(item(i*2+1).txt_size == size);
		ok1(!strncmp(item(i*2+1).txt, string + i*(size+1), size));
		ok1(item(i*2+1).orig_size == size);
		ok1(item(i*2+1).orig == string + i*(size+1));
		ok1(item(i*2+2).type == TOK_WHITE);
		ok1(item(i*2+2).txt_size == 1);
		ok1(item(i*2+2).txt[0] == ' ');
		ok1(item(i*2+2).orig_size == 1);
		ok1(item(i*2+2).orig == string + i*(size+1) + size);
	}
	return toks;
}

static struct token_list *test_tokens_backslashed(const char *orig,
						  unsigned int size)
{
	struct token_list *toks;
	const char *string = backslashify(orig);
	unsigned int i;

	toks = tokenize(string, string, strlen(string), MQ);
	ok1(token_list_sanity_check(toks, stdout));
	
	ok1(token_list_count(toks) == strlen(orig)/size + 1);
	ok1(item(0).type == TOK_STARTLINE);
	for (i = 0; i < strlen(orig)/size; i++) {
		ok1(item(i+1).type == TOK_OPERATOR);
		ok1(item(i+1).txt_size == size);
		ok1(strncmp(item(i+1).txt, orig + i*size, size) == 0);
		ok1(item(i+1).orig_size == size*3);
		ok1(item(i+1).orig == string + i*size*3);
	}
	return toks;
}

static void onechar_checks(const struct token_list *toks, int mul)
{
	unsigned int i;
	for (i = 0; i < strlen(onechar_tokens); i++)
		ok1(item(i*mul+1).opkw == onechar_tokens[i]);
}

static void twochar_checks(const struct token_list *toks, int mul)
{
	ok1(item(1).opkw == NE_OP);
	ok1(item(1*mul+1).opkw == DOUBLE_POUND);
	ok1(item(2*mul+1).opkw == MOD_ASSIGN);
	ok1(item(3*mul+1).opkw == XOR_ASSIGN);
	ok1(item(4*mul+1).opkw == AND_ASSIGN);
	ok1(item(5*mul+1).opkw == AND_OP);
	ok1(item(6*mul+1).opkw == MUL_ASSIGN);
	ok1(item(7*mul+1).opkw == SUB_ASSIGN);
	ok1(item(8*mul+1).opkw == DEC_OP);
	ok1(item(9*mul+1).opkw == PTR_OP);
	ok1(item(10*mul+1).opkw == ADD_ASSIGN);
	ok1(item(11*mul+1).opkw == INC_OP);
	ok1(item(12*mul+1).opkw == EQ_OP);
	ok1(item(13*mul+1).opkw == OR_ASSIGN);
	ok1(item(14*mul+1).opkw == OR_OP);
	ok1(item(15*mul+1).opkw == LE_OP);
	ok1(item(16*mul+1).opkw == LEFT_OP);
	ok1(item(17*mul+1).opkw == GE_OP);
	ok1(item(18*mul+1).opkw == RIGHT_OP);
	ok1(item(19*mul+1).opkw == DIV_ASSIGN);
}

static void threechar_checks(const struct token_list *toks, int mul)
{
	ok1(item(1).opkw == LEFT_ASSIGN);
	ok1(item(1*mul+1).opkw == RIGHT_ASSIGN);
	ok1(item(2*mul+1).opkw == ELLIPSIS);
}

int main(void)
{
	unsigned int i;
	struct token_list *toks;
	char *str;
	char *backslashed_idents;

	plan_tests(1243);
	toks = test_tokens(onechar_tokens, 1);
	onechar_checks(toks, 1);
	talloc_free((char*)toks->orig);

	toks = test_tokens(twochar_tokens, 2);
	twochar_checks(toks, 1);
	talloc_free((char*)toks->orig);

	toks = test_tokens(threechar_tokens, 3);
	threechar_checks(toks, 1);
	talloc_free((char*)toks->orig);

	/* char literal */
	str = talloc_strdup(NULL, char_token);
	toks = tokenize(str, str, strlen(str), MQ);
	ok1(token_list_sanity_check(toks, stdout));
	ok1(token_list_count(toks) == 2);
	ok1(item(0).type == TOK_STARTLINE);
	ok1(item(1).type == TOK_CHAR);
	ok1(item(1).txt_size == strlen(str));
	ok1(strncmp(item(1).txt, str, strlen(str)) == 0);
	ok1(item(1).orig_size == strlen(str));
	ok1(item(1).orig == str);
	/* FIXME: test contents of string. */
	talloc_free(str);

	/* string literal */
	str = talloc_strdup(NULL, string_token);
	toks = tokenize(str, str, strlen(str), MQ);
	ok1(token_list_sanity_check(toks, stdout));
	ok1(token_list_count(toks) == 2);
	ok1(item(0).type == TOK_STARTLINE);
	ok1(item(1).type == TOK_STRING);
	ok1(item(1).txt_size == strlen(str));
	ok1(strncmp(item(1).txt, str, strlen(str)) == 0);
	ok1(item(1).orig_size == strlen(str));
	ok1(item(1).orig == str);
	/* FIXME: test contents of string. */
	talloc_free(str);

	/* Identifiers */
	str = talloc_strdup(NULL, ident_tokens);
	toks = tokenize(str, str, strlen(str), MQ);
	ok1(token_list_sanity_check(toks, stdout));
	token_list_dump(toks, stdout);
	ok1(token_list_count(toks) == 10);
	ok1(item(0).type == TOK_STARTLINE);
	for (i = 0; i < 5; i++) {
		ok1(item(i*2+1).type == TOK_IDENTIFIER);
		ok1(item(i*2+1).txt_size == 3);
		ok1(strncmp(item(i*2+1).txt, str + i*4, 3) == 0);
		ok1(item(i*2+1).orig_size == 3);
		ok1(item(i*2+1).orig == str + i*4);
		if (i == 4)
			continue;
		ok1(item(i*2+2).type == TOK_WHITE);
		ok1(item(i*2+2).txt_size == 1);
		ok1(item(i*2+2).txt[0] == ' ');
		ok1(item(i*2+2).orig_size == 1);
		ok1(item(i*2+2).orig == str + i*4 + 3);
	}
	talloc_free(str);

	toks = test_tokens_spaced(onechar_tokens, 1);
	onechar_checks(toks, 2);
	talloc_free((char*)toks->orig);

	toks = test_tokens_spaced(twochar_tokens, 2);
	twochar_checks(toks, 2);
	talloc_free((char*)toks->orig);

	toks = test_tokens_spaced(threechar_tokens, 3);
	threechar_checks(toks, 2);
	talloc_free((char*)toks->orig);

	toks = test_tokens_backslashed(onechar_tokens, 1);
	onechar_checks(toks, 1);
	talloc_free((char*)toks->orig);

	toks = test_tokens_backslashed(twochar_tokens, 2);
	twochar_checks(toks, 1);
	talloc_free((char*)toks->orig);

	toks = test_tokens_backslashed(threechar_tokens, 3);
	threechar_checks(toks, 1);
	talloc_free((char*)toks->orig);

	/* Identifiers */
	backslashed_idents = backslashify(ident_tokens);
	toks = tokenize(backslashed_idents, backslashed_idents, strlen(backslashed_idents), MQ);
	ok1(token_list_sanity_check(toks, stdout));
	ok1(token_list_count(toks) == 10);
	ok1(item(0).type == TOK_STARTLINE);
	for (i = 0; i < 5; i++) {
		ok1(item(i*2+1).type == TOK_IDENTIFIER);
		ok1(item(i*2+1).txt_size == 3);
		ok1(strncmp(item(i*2+1).txt, ident_tokens + i*4, 3) == 0);
		ok1(item(i*2+1).orig_size == 9);
		ok1(item(i*2+1).orig == backslashed_idents + i*12);
		if (i == 4)
			continue;
		ok1(item(i*2+2).type == TOK_WHITE);
		ok1(item(i*2+2).txt_size == 1);
		ok1(item(i*2+2).txt[0] == ' ');
		ok1(item(i*2+2).orig_size == 3);
		ok1(item(i*2+2).orig == backslashed_idents + i*12 + 9);
	}
	talloc_free(backslashed_idents);

	return exit_status();
}
