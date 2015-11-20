/* vim: set ai sw=8 sts=8 ts=8 : */

#include "ep.h"
#include "ep_crypto.h"
#include "ep_dbg.h"

#include <openssl/pem.h>
#if _EP_CRYPTO_INCLUDE_RSA
# include <openssl/rsa.h>
#endif
#if _EP_CRYPTO_INCLUDE_DSA
# include <openssl/dsa.h>
#endif
#if _EP_CRYPTO_INCLUDE_EC
# include <openssl/ec.h>
#endif
#if _EP_CRYPTO_INCLUDE_DH
# include <openssl/dh.h>
#endif

#include <string.h>


static EP_DBG	Dbg = EP_DBG_INIT("libep.crypto.key", "Cryptographic key utilities");

// unclear in what version BIO_s_secmem appears....
#define BIO_s_secmem	BIO_s_mem


#if _EP_CRYPTO_INCLUDE_RSA
/*
**  Create new RSA key pair
**
**	The keyexp parameter is the exponent parameter for RSA.
**	If zero a default is used.
*/

static EP_STAT
generate_rsa_key(EP_CRYPTO_KEY *key, int keylen, int keyexp)
{
	RSA *rsakey;

	if (keyexp <= 0)
		keyexp = ep_adm_getintparam("libep.crypto.rsa.key.exponent",
					3);
	if (keylen < EP_CRYPTO_KEY_MINLEN_RSA)
	{
		_ep_crypto_error("insecure RSA key length %d; %d min",
				keylen, EP_CRYPTO_KEY_MINLEN_RSA);
		goto fail0;
	}
	rsakey = RSA_generate_key(keylen, keyexp, NULL, NULL);
	if (rsakey == NULL)
	{
		_ep_crypto_error("cannot generate RSA key");
		goto fail0;
	}
	if (EVP_PKEY_assign_RSA(key, rsakey) != 1)
	{
		_ep_crypto_error("cannot save RSA key");
		goto fail0;
	}

	return EP_STAT_OK;

fail0:
	return EP_STAT_CRYPTO_KEYCREATE;
}
#endif

#if _EP_CRYPTO_INCLUDE_DSA
/*
**  Generate new DSA key pair.
*/

static EP_STAT
generate_dsa_key(EP_CRYPTO_KEY *key, int keylen)
{
	DSA *dsakey;

	// generate new parameter block
	dsakey = DSA_new();
	if (DSA_generate_parameters_ex(dsakey, keylen,
			NULL, 0, NULL, NULL, NULL) != 1)
	{
		_ep_crypto_error("cannot initialize DSA parameters");
		goto fail0;
	}

	if (DSA_generate_key(dsakey) != 1)
	{
		_ep_crypto_error("cannot generate DSA key");
		goto fail0;
	}
	if (EVP_PKEY_assign_DSA(key, dsakey) != 1)
	{
		_ep_crypto_error("cannot save DSA key");
		goto fail0;
	}

	return EP_STAT_OK;

fail0:
	return EP_STAT_CRYPTO_KEYCREATE;
}
#endif

#if _EP_CRYPTO_INCLUDE_DH
/*
**  Generate new Diffie-Hellman key.
**	XXX NOT IMPLEMENTED AT THIS TIME
*/

static EP_STAT
generate_dh_key(EP_CRYPTO_KEY *key, ...)
{
	DH *dhkey = DH_generate_key(keylen, XXX);
	if (dhkey == NULL)
		return _ep_crypto_error("cannot generate DH key");
	if (EVP_PKEY_assign_DH(key, dhkey) != 1)
		return _ep_crypto_error("cannot save DH key");
}
#endif

#if _EP_CRYPTO_INCLUDE_EC
/*
**  Generate new elliptic curve key pair.
**	The curve parameter is passed in as a string.
**	If NULL then a default is used.
*/

static EP_STAT
generate_ec_key(EP_CRYPTO_KEY *key, const char *curve)
{
	if (curve == NULL)
		curve = ep_adm_getstrparam("libep.crypto.key.ec.curve",
				"sect283r1");
	int nid = OBJ_txt2nid(curve);
	if (nid == NID_undef)
	{
		_ep_crypto_error("unknown EC curve name %s", curve);
		goto fail0;
	}
	EC_KEY *eckey = EC_KEY_new_by_curve_name(nid);
	if (eckey == NULL)
	{
		_ep_crypto_error("cannot create EC key");
		goto fail0;
	}
	if (!EC_KEY_generate_key(eckey))
	{
		_ep_crypto_error("cannot generate EC key");
		goto fail1;
	}
	if (EVP_PKEY_assign_EC_KEY(key, eckey) != 1)
	{
		_ep_crypto_error("cannot assign EC key");
		goto fail1;
	}
	return EP_STAT_OK;

fail1:
	EC_KEY_free(eckey);

fail0:
	return EP_STAT_CRYPTO_KEYCREATE;
}
#endif

EP_CRYPTO_KEY *
ep_crypto_key_create(int keytype, int keylen, int keyexp, const char *curve)
{
	EVP_PKEY *key;
	EP_STAT estat;

	key = EVP_PKEY_new();
	if (key == NULL)
		return _ep_crypto_error("Cannot create new keypair");

	switch (keytype)
	{
#if _EP_CRYPTO_INCLUDE_RSA
	  case EP_CRYPTO_KEYTYPE_RSA:
		estat = generate_rsa_key(key, keylen, keyexp);
		break;
#endif

#if _EP_CRYPTO_INCLUDE_DSA
	  case EP_CRYPTO_KEYTYPE_DSA:
		estat = generate_dsa_key(key, keylen);
		break;
#endif

#if _EP_CRYPTO_INCLUDE_DH
	  case EP_CRYPTO_KEYTYPE_DH:
		estat = generate_dh_key(key, ...);
		break;
#endif

#if _EP_CRYPTO_INCLUDE_EC
	  case EP_CRYPTO_KEYTYPE_EC:
		estat = generate_ec_key(key, curve);
		break;
#endif

	  default:
		return _ep_crypto_error("unrecognized key type %d", keytype);
	}
	if (EP_STAT_ISOK(estat))
		return key;
	EVP_PKEY_free(key);
	return NULL;
}


/*
**  Free a cryptographic key
*/

void
ep_crypto_key_free(EP_CRYPTO_KEY *key)
{
	EVP_PKEY_free(key);
}


/*
**  Generic routines to map external to internal names and back.
**	Assumes short tables --- linear search.
*/

struct name_to_format
{
	const char	*str;		// string name of format/algorithm
	int		form;		// internal name
};


static int
get_byname(struct name_to_format *table, const char *s)
{
	struct name_to_format *kt;

	for (kt = table; kt->str != NULL; kt++)
	{
		if (strcasecmp(kt->str, s) == 0)
			break;
	}
	return kt->form;
}

static const char *
get_name(struct name_to_format *table, int code)
{
	struct name_to_format *kt;

	for (kt = table; kt->str != NULL; kt++)
	{
		if (kt->form == code)
			return kt->str;
	}
	return "unknown";
}


/*
**  Map key format external->internal
*/

static struct name_to_format	KeyFormStrings[] =
{
	{ "pem",		EP_CRYPTO_KEYFORM_PEM,		},
	{ "der",		EP_CRYPTO_KEYFORM_DER,		},
	{ NULL,			0				}
};

int
ep_crypto_keyform_fromstring(const char *fmt)
{
	return get_byname(KeyFormStrings, fmt);
}


/*
**  Map EP cipher index to OpenSSL cipher structure
*/

static const EVP_CIPHER *
cipher_byid(int id)
{
	if (id == EP_CRYPTO_SYMKEY_NONE)
		return NULL;

	// a bit weird, but easier to get cipher by name
	const char *s = ep_crypto_keyenc_name(id);
	if (s == NULL)
		return NULL;
	const EVP_CIPHER *enc = EVP_get_cipherbyname(s);
	if (enc == NULL)
		_ep_crypto_error("keyenc_byid: unknown EVP cipher %s", s);
	return enc;
}


/*
**  Print a key to a given file
**
**	If EP_CRYPTO_F_SECRET bit is set in flags, it prints the
**	secret key; otherwise it prints the public key.
*/

void
ep_crypto_key_print(
		EP_CRYPTO_KEY *key,
		FILE *fp,
		uint32_t flags)
{
	BIO *bio = BIO_new_fp(fp, BIO_NOCLOSE);
	int istat;
	int off = 0;

	if (bio == NULL)
	{
		_ep_crypto_error("ep_crypto_key_print: cannot allocate bio");
		return;
	}
	if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
		istat = EVP_PKEY_print_private(bio, key, off, NULL);
	else
		istat = EVP_PKEY_print_public(bio, key, off, NULL);
	if (istat != 1)
		_ep_crypto_error("ep_crypto_key_print: cannot print");
	BIO_free(bio);
}


/*
**  Read a key in either PEM or DER format from an OpenSSL bio structure.
*/

static EP_CRYPTO_KEY *
key_read_bio(BIO *bio,
		const char *filename,
		int keyform,
		uint32_t flags)
{
	EVP_PKEY *key = NULL;
	const char *pubsec = EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags) ?
		"secret" : "public";

	ep_dbg_cprintf(Dbg, 20, "key_read_bio: name %s, form %d, flags %x\n",
			filename, keyform, flags);
	EP_ASSERT(bio != NULL);
	if (keyform <= 0)
		return _ep_crypto_error("keyform must be specified");

	if (keyform == EP_CRYPTO_KEYFORM_PEM)
	{
		// easy case
		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
			key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
		else
			key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
	}
	else if (keyform == EP_CRYPTO_KEYFORM_DER)
	{
		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
			key = d2i_PrivateKey_bio(bio, NULL);
		else
			key = d2i_PUBKEY_bio(bio, NULL);
	}
	else
	{
		return _ep_crypto_error("unknown key format %d", keyform);
	}

	if (key == NULL)
		return _ep_crypto_error("cannot read %s key from %s",
				pubsec, filename);
	return key;
}


/*
**  Read a key from a file pointer, a file, or memory
**
**	The filename passed in to the file pointer version is used
**	only for printing errors.
*/

EP_CRYPTO_KEY *
ep_crypto_key_read_fp(
		FILE *fp,
		const char *filename,
		int keyform,
		uint32_t flags)
{
	EP_CRYPTO_KEY *key;
	BIO *bio = BIO_new_fp(fp, BIO_NOCLOSE);

	key = key_read_bio(bio, filename, keyform, flags);
	BIO_free(bio);
	return key;
}


EP_CRYPTO_KEY *
ep_crypto_key_read_file(
		const char *filename,
		int keyform,
		uint32_t flags)
{
	FILE *fp;
	EP_CRYPTO_KEY *key;

	// determine key format from file name (heuristic)
	if (keyform <= 0)
	{
		const char *p = strrchr(filename, '.');

		if (p != NULL)
			keyform = ep_crypto_keyform_fromstring(++p);
	}
	if (keyform <= 0)
		return _ep_crypto_error("keyform must be specified");

	fp = fopen(filename, "r");
	if (fp == NULL)
		return _ep_crypto_error("cannot open key file %s", filename);
	key = ep_crypto_key_read_fp(fp, filename, keyform, flags);
	fclose(fp);
	return key;
}


EP_CRYPTO_KEY *
ep_crypto_key_read_mem(
		const void *buf,
		size_t buflen,
		int keyform,
		uint32_t flags)
{
	EP_CRYPTO_KEY *key;
	BIO *bio = BIO_new_mem_buf((void *) buf, buflen);

	key = key_read_bio(bio, "memory", keyform, flags);
	BIO_free(bio);
	return key;
}


/*
**  Write a key to a BIO output (internal use)
**
**	Have to use BIO because it's the only way to find out how
**	long the output is (that I could find).
**
**	For PEM the calling convention is:
**	    PEM_write_*(fp, key, keyenc, kstr, klen, pwcb, u)
**	where keyenc is the key encryption cipher, kstr & klen point
**	at the password, pwcb points to a routine to use to read a
**	password, and u is a parameter to pwcb.  If kstr and pwcb
**	are both NULL, u can be a pointer to a null-terminated
**	password string.
*/

static EP_STAT
key_write_bio(EP_CRYPTO_KEY *key,
		BIO *bio,
		int keyform,
		int keyenc,
		const char *passwd,
		uint32_t flags)
{
	int keytype = ep_crypto_keytype_fromkey(key);
	const char *pubsec = EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags) ?
		"secret" : "public";
	int istat;

	EP_ASSERT(bio != NULL);

	if (keyform <= 0)
	{
		(void) _ep_crypto_error("keyform must be specified");
		return EP_STAT_CRYPTO_CONVERT;
	}

	if (keyform == EP_CRYPTO_KEYFORM_PEM)
	{
		// easy case
		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
		{
			const EVP_CIPHER *enc = cipher_byid(keyenc);

			istat = PEM_write_bio_PrivateKey(bio, key, enc,
					NULL, 0, NULL, (void *) passwd);
		}
		else
		{
			istat = PEM_write_bio_PUBKEY(bio, key);
		}
		if (istat != 1)
		{
			(void) _ep_crypto_error("cannot write %s PEM key",
					pubsec);
			return EP_STAT_CRYPTO_CONVERT;
		}
		goto finis;
	}
	else if (keyform != EP_CRYPTO_KEYFORM_DER)
	{
		(void) _ep_crypto_error("unknown key format %d", keyform);
		return EP_STAT_CRYPTO_KEYFORM;
	}

#if _EP_CRYPTO_INCLUDE_DER
	const char *type;
	switch (keytype)
	{
# if _EP_CRYPTO_INCLUDE_RSA
	  case EP_CRYPTO_KEYTYPE_RSA:
		type = "RSA";
		RSA *rsakey = EVP_PKEY_get1_RSA(key);

		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
			istat = i2d_RSAPrivateKey_bio(bio, rsakey);
		else
			istat = i2d_RSA_PUBKEY_bio(bio, rsakey);
		if (istat != 1)
			goto fail1;
		break;
# endif

# if _EP_CRYPTO_INCLUDE_DSA
	  case EP_CRYPTO_KEYTYPE_DSA:
		type = "DSA";
		DSA *dsakey = EVP_PKEY_get1_DSA(key);

		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
			istat = i2d_DSAPrivateKey_bio(bio, dsakey);
		else
			istat = i2d_DSA_PUBKEY_bio(bio, dsakey);
		if (istat != 1)
			goto fail1;
		break;
# endif

# if _EP_CRYPTO_INCLUDE_EC
	  case EP_CRYPTO_KEYTYPE_EC:
		type = "ECDSA";
		EC_KEY *eckey = EVP_PKEY_get1_EC_KEY(key);

		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
			istat = i2d_ECPrivateKey_bio(bio, eckey);
		else
			istat = i2d_EC_PUBKEY_bio(bio, eckey);
		if (istat != 1)
			goto fail1;
		break;
# endif

# if _EP_CRYPTO_INCLUDE_DH
	  case EP_CRYPTO_KEYTYPE_DH:
		type = "DH";
		DH *dhkey = EVP_PKEY_get1_DH(key);
		do what is needed
		if (istat != 1)
			goto fail1;
		break;
# endif

	  default:
		_ep_crypto_error("unknown key type %d", keytype);
		return EP_STAT_CRYPTO_KEYTYPE;
	}
#endif // _EP_CRYPTO_INCLUDE_DER

finis:
	return EP_STAT_FROM_INT(BIO_ctrl_pending(bio));

fail1:
	(void) _ep_crypto_error("cannot encode %s %s DER key", type, pubsec);
	return EP_STAT_CRYPTO_CONVERT;
}


/*
**  Write keys to files or memory.
**
**	If EP_CRYPTO_F_SECRET bit is set in flags, it writes the
**	secret key; otherwise it writes the public key.
*/

EP_STAT
ep_crypto_key_write_fp(EP_CRYPTO_KEY *key,
		FILE *fp,
		int keyform,
		int keyenc,
		const char *passwd,
		uint32_t flags)
{
	EP_STAT estat;
	BIO *bio;

	if (fp == NULL)
	{
		(void) _ep_crypto_error("file must be specified");
		return EP_STAT_CRYPTO_CONVERT;
	}

	bio = BIO_new_fp(fp, BIO_NOCLOSE);
	estat = key_write_bio(key, bio, keyform, keyenc, passwd, flags);
	BIO_free(bio);
	return estat;
}


EP_STAT
ep_crypto_key_write_mem(EP_CRYPTO_KEY *key,
		void *buf,
		size_t buflen,
		int keyform,
		int keyenc,
		const char *passwd,
		uint32_t flags)
{
	EP_STAT estat;
	BIO *bio;

	if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
		bio = BIO_new(BIO_s_secmem());
	else
		bio = BIO_new(BIO_s_mem());
	estat = key_write_bio(key, bio, keyform, keyenc, passwd, flags);
	if (EP_STAT_ISOK(estat))
	{
		// save the memory
		size_t len = EP_STAT_TO_INT(estat);

		if (len > buflen)
		{
			(void) _ep_crypto_error("external keyform too long, wants %z, needs %z",
					len, buflen);
			estat = EP_STAT_CRYPTO_CONVERT;
		}
		else
		{
			void *p;

			BIO_get_mem_data(bio, &p);
			memcpy(buf, p, len);
		}
	}

	BIO_free(bio);
	return estat;
}


/*
**  Extract the EP key type from an EP key.
*/

int
ep_crypto_keytype_fromkey(EP_CRYPTO_KEY *key)
{
	int i = EVP_PKEY_type(key->type);

	switch (i)
	{
	  case EVP_PKEY_RSA:
		return EP_CRYPTO_KEYTYPE_RSA;

	  case EVP_PKEY_DSA:
		return EP_CRYPTO_KEYTYPE_DSA;

	  case EVP_PKEY_DH:
		return EP_CRYPTO_KEYTYPE_DH;

	  case EVP_PKEY_EC:
		return EP_CRYPTO_KEYTYPE_EC;

	  default:
		return EP_CRYPTO_KEYTYPE_UNKNOWN;
	}
}


/*
**  Convert an EP key type from or to a printable name
*/

static struct name_to_format	KeyTypeStrings[] =
{
	{ "rsa",		EP_CRYPTO_KEYTYPE_RSA,		},
	{ "dsa",		EP_CRYPTO_KEYTYPE_DSA,		},
	{ "ec",			EP_CRYPTO_KEYTYPE_EC,		},
	{ "ecdsa",		EP_CRYPTO_KEYTYPE_EC,		},
	{ "dh",			EP_CRYPTO_KEYTYPE_DH,		},
	{ NULL,			EP_CRYPTO_KEYTYPE_UNKNOWN,	}
};

int
ep_crypto_keytype_byname(const char *fmt)
{
	return get_byname(KeyTypeStrings, fmt);
}

const char *
ep_crypto_keytype_name(int keytype)
{
	return get_name(KeyTypeStrings, keytype);
}


/*
**  Check to see if two keys are compatible.
**
**	By default this checks to see if the public and secret key
**	are two halves of the same key pair.  If you set the
**	libep.crypto-key.compat.strict admin parameter to false it
**	just checks to make sure they are the same type (e.g.,
**	both EC rather than one EC and one RSA).  This is really
**	only used for debugging.
*/

EP_STAT
ep_crypto_key_compat(const EP_CRYPTO_KEY *pubkey, const EP_CRYPTO_KEY *seckey)
{
	int istat = EVP_PKEY_cmp(pubkey, seckey);
	int icheck;

	// setting this param to false is mostly for debugging
	if (ep_adm_getboolparam("libep.crypto.key.compat.strict", true))
		icheck = 1;	// check to see that key halves match
	else
		icheck = 0;	// just check to see if they are the same type
	if (istat >= icheck)
		return EP_STAT_OK;
	return EP_STAT_CRYPTO_KEYCOMPAT;
}


/*
**  Convert a symmetric key name to or from an internal EP type.
*/

static struct name_to_format	KeyEncStrings[] =
{
	{ "none",		EP_CRYPTO_SYMKEY_NONE,		},
	{ "aes128",		EP_CRYPTO_SYMKEY_AES128,	},
	{ "aes192",		EP_CRYPTO_SYMKEY_AES192,	},
	{ "aes256",		EP_CRYPTO_SYMKEY_AES256,	},
	{ "camellia128",	EP_CRYPTO_SYMKEY_CAMELLIA128,	},
	{ "camellia192",	EP_CRYPTO_SYMKEY_CAMELLIA192,	},
	{ "camellia256",	EP_CRYPTO_SYMKEY_CAMELLIA256,	},
	{ "des",		EP_CRYPTO_SYMKEY_DES,		},
	{ "3des",		EP_CRYPTO_SYMKEY_3DES,		},
	{ "idea",		EP_CRYPTO_SYMKEY_IDEA,		},
	{ NULL,			-1,				}
};

int
ep_crypto_keyenc_byname(const char *str)
{
	int enc = get_byname(KeyEncStrings, str);

	// if openssl doesn't know about it, return an error
	if (enc > 0 && EVP_get_cipherbyname(str) == NULL)
		return -1;
	return get_byname(KeyEncStrings, str);
}

const char *
ep_crypto_keyenc_name(int key_enc_alg)
{
	return get_name(KeyEncStrings, key_enc_alg);
}
