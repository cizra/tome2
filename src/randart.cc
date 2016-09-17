/*
 * Copyright (c) 2001 DarkGod
 *
 * This software may be copied and distributed for educational, research, and
 * not for profit purposes provided that this copyright and statement are
 * included in all such copies.
 */

#include "randart.hpp"
#include "mimic.hpp"
#include "object1.hpp"
#include "object2.hpp"
#include "object_flag.hpp"
#include "object_type.hpp"
#include "options.hpp"
#include "player_type.hpp"
#include "quark.hpp"
#include "randart_gen_type.hpp"
#include "randart_part_type.hpp"
#include "spells2.hpp"
#include "util.hpp"
#include "variable.h"
#include "variable.hpp"
#include "z-rand.hpp"

#include <memory>
#include <vector>

/* Chance of using syllables to form the name instead of the "template" files */
#define TABLE_NAME      45
#define A_CURSED        13
#define WEIRD_LUCK      12
#define ACTIVATION_CHANCE 3

/*
 * Attempt to add a power to a randart
 */
static bool_ grab_one_power(int *ra_idx, object_type *o_ptr, bool_ good, s16b *max_times)
{
	bool_ ret = FALSE;

	std::vector<size_t> ok_ra;

	/* Grab the ok randart */
	for (size_t i = 0; i < max_ra_idx; i++)
	{
		randart_part_type *ra_ptr = &ra_info[i];
		bool_ ok = FALSE;

		/* Must have the correct fields */
		for (size_t j = 0; j < 20; j++)
		{
			if (ra_ptr->tval[j] == o_ptr->tval)
			{
				if ((ra_ptr->min_sval[j] <= o_ptr->sval) && (ra_ptr->max_sval[j] >= o_ptr->sval)) ok = TRUE;
			}

			if (ok) break;
		}
		if ((0 < ra_ptr->max_pval) && (ra_ptr->max_pval < o_ptr->pval)) ok = FALSE;
		if (!ok)
		{
			/* Doesnt count as a try*/
			continue;
		}

		/* Good should be good, bad should be bad */
		if (good && (ra_ptr->value <= 0)) continue;
		if ((!good) && (ra_ptr->value > 0)) continue;

		if (max_times[i] >= ra_ptr->max) continue;

		/* Must NOT have the antagonic flags */
		auto const flags = object_flags(o_ptr);
		if (flags & ra_ptr->aflags) continue;

		/* ok */
		ok_ra.push_back(i);
	}

	/* Now test them a few times */
	for (size_t count = 0; count < ok_ra.size() * 10; count++)
	{
		size_t i = ok_ra[rand_int(ok_ra.size())];
		randart_part_type *ra_ptr = &ra_info[i];

		/* XXX XXX Enforce minimum player level (loosely) */
		if (ra_ptr->level > p_ptr->lev)
		{
			/* Acquire the "out-of-depth factor" */
			int d = (ra_ptr->level - p_ptr->lev);

			/* Roll for out-of-depth creation */
			if (rand_int(d) != 0)
			{
				continue;
			}
		}

		/* We must make the "rarity roll" */
		if (rand_int(ra_ptr->mrarity) < ra_ptr->rarity)
		{
			continue;
		}

		/* Hack -- mark the item as an ego */
		*ra_idx = i;
		max_times[i]++;

		/* Success */
		ret = TRUE;
		break;
	}

	/* Return */
	return (ret);
}

void give_activation_power (object_type * o_ptr)
{
	o_ptr->xtra2 = 0;
	o_ptr->art_flags &= ~TR_ACTIVATE;
	o_ptr->timeout = 0;
}


int get_activation_power()
{
	object_type *o_ptr, forge;

	o_ptr = &forge;

	give_activation_power(o_ptr);

	return o_ptr->xtra2;
}

#define MIN_NAME_LEN 5
#define MAX_NAME_LEN 9
#define S_WORD 26
#define E_WORD S_WORD

static long lprobs[S_WORD + 1][S_WORD + 1][S_WORD + 1];
static long ltotal[S_WORD + 1][S_WORD + 1];

/*
 * Use W. Sheldon Simms' random name generator.  This function builds
 * probability tables which are used later on for letter selection.  It
 * relies on the ASCII character set.
 */
void build_prob(cptr learn)
{
	int c_prev, c_cur, c_next;

	/* Build raw frequencies */
	while (1)
	{
		c_prev = c_cur = S_WORD;

		do
		{
			c_next = *learn++;
		}
		while (!isalpha(c_next) && (c_next != '\0'));

		if (c_next == '\0') break;

		do
		{
			c_next = A2I(tolower(c_next));
			lprobs[c_prev][c_cur][c_next]++;
			ltotal[c_prev][c_cur]++;
			c_prev = c_cur;
			c_cur = c_next;
			c_next = *learn++;
		}
		while (isalpha(c_next));

		lprobs[c_prev][c_cur][E_WORD]++;
		ltotal[c_prev][c_cur]++;
	}
}


/*
 * Use W. Sheldon Simms' random name generator.  Generate a random word using
 * the probability tables we built earlier.  Relies on the ASCII character
 * set.  Relies on European vowels (a, e, i, o, u).  The generated name should
 * be copied/used before calling this function again.
 */
static char *make_word(void)
{
	static char word_buf[90];
	int r, totalfreq;
	int tries, lnum, vow;
	int c_prev, c_cur, c_next;
	char *cp;

startover:
	vow = 0;
	lnum = 0;
	tries = 0;
	cp = word_buf;
	c_prev = c_cur = S_WORD;

	while (1)
	{
getletter:
		c_next = 0;
		r = rand_int(ltotal[c_prev][c_cur]);
		totalfreq = lprobs[c_prev][c_cur][c_next];

		while (totalfreq <= r)
		{
			c_next++;
			totalfreq += lprobs[c_prev][c_cur][c_next];
		}

		if (c_next == E_WORD)
		{
			if ((lnum < MIN_NAME_LEN) || vow == 0)
			{
				tries++;
				if (tries < 10) goto getletter;
				goto startover;
			}
			*cp = '\0';
			break;
		}

		if (lnum >= MAX_NAME_LEN) goto startover;

		*cp = I2A(c_next);

		if (is_a_vowel(*cp)) vow++;

		cp++;
		lnum++;
		c_prev = c_cur;
		c_cur = c_next;
	}

	word_buf[0] = toupper(word_buf[0]);

	return (word_buf);
}


void get_random_name(char * return_name)
{
	char *word = make_word();

	if (rand_int(3) == 0)
		sprintf(return_name, "'%s'", word);
	else
		sprintf(return_name, "of %s", word);
}


bool_ create_artifact(object_type *o_ptr, bool_ a_scroll, bool_ get_name)
{
	char new_name[80];
	int powers = 0, i;
	s32b total_flags, total_power = 0;
	bool_ a_cursed = FALSE;
	s16b pval = 0;
	bool_ limit_blows = FALSE;

	strcpy(new_name, "");

	if ((!a_scroll) && (randint(A_CURSED) == 1)) a_cursed = TRUE;

	i = 0;
	while (ra_gen[i].chance)
	{
		powers += damroll(ra_gen[i].dd, ra_gen[i].ds) + ra_gen[i].plus;
		i++;
	}

	if ((!a_cursed) && (randint(30) == 1)) powers *= 2;

	if (a_cursed) powers /= 2;

	std::unique_ptr<s16b[]> max_times(new s16b[max_ra_idx]);
	for (int i = 0; i < max_ra_idx; i++) {
		max_times[i] = 0;
	}

	/* Main loop */
	while (powers)
	{
		int ra_idx;
		randart_part_type *ra_ptr;

		powers--;

		if (!grab_one_power(&ra_idx, o_ptr, TRUE, max_times.get())) continue;

		ra_ptr = &ra_info[ra_idx];

		if (wizard) msg_format("Adding randart power: %d", ra_idx);

		total_power += ra_ptr->value;

		o_ptr->art_flags |= ra_ptr->flags;

		add_random_ego_flag(o_ptr, ra_ptr->fego, &limit_blows);

		/* get flags */
		auto const flags = object_flags(o_ptr);

		/* Hack -- acquire "cursed" flag */
		if (flags & TR_CURSED) o_ptr->ident |= (IDENT_CURSED);

		/* Hack -- obtain bonuses */
		if (ra_ptr->max_to_h > 0) o_ptr->to_h += randint(ra_ptr->max_to_h);
		if (ra_ptr->max_to_h < 0) o_ptr->to_h -= randint( -ra_ptr->max_to_h);
		if (ra_ptr->max_to_d > 0) o_ptr->to_d += randint(ra_ptr->max_to_d);
		if (ra_ptr->max_to_d < 0) o_ptr->to_d -= randint( -ra_ptr->max_to_d);
		if (ra_ptr->max_to_a > 0) o_ptr->to_a += randint(ra_ptr->max_to_a);
		if (ra_ptr->max_to_a < 0) o_ptr->to_a -= randint( -ra_ptr->max_to_a);

		/* Hack -- obtain pval */
		if (((pval > ra_ptr->max_pval) && ra_ptr->max_pval) || (!pval)) pval = ra_ptr->max_pval;
	};

	if (pval > 0) o_ptr->pval = randint(pval);
	if (pval < 0) o_ptr->pval = randint( -pval);

	/* No insane number of blows */
	if (limit_blows && (o_ptr->art_flags & TR_BLOWS))
	{
		if (o_ptr->pval > 2) o_ptr->pval = randint(2);
	}

	/* Just to be sure */
	o_ptr->art_flags |=
		TR_IGNORE_ACID |
		TR_IGNORE_ELEC |
		TR_IGNORE_FIRE |
		TR_IGNORE_COLD;

	total_flags = flag_cost(o_ptr, o_ptr->pval);
	if (options->cheat_peek)
	{
		msg_format("%ld", total_flags);
	}

	if (a_cursed) curse_artifact(o_ptr);

	if (get_name)
	{
		if (a_scroll)
		{
			char dummy_name[80];

			/* Identify it fully */
			object_aware(o_ptr);
			object_known(o_ptr);
			o_ptr->ident |= (IDENT_STOREB | IDENT_MENTAL);

			strcpy(dummy_name, "");
			object_out_desc(o_ptr, NULL, FALSE, TRUE);

			if (get_string("What do you want to call the artifact? ", dummy_name, 80))
			{
				strcpy(new_name, "called '");
				strcat(new_name, dummy_name);
				strcat(new_name, "'");
			}
			else
				/* Default name = of 'player name' */
				sprintf(new_name, "of '%s'", player_name);
		}
		else
		{
			get_random_name(new_name);
		}
	}

	/* Save the inscription */
	o_ptr->art_name = quark_add(new_name);
	o_ptr->name2 = o_ptr->name2b = 0;

	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP);

	/* Extract the flags */
	auto const flags = object_flags(o_ptr);

	/* HACKS for ToME */
	if (o_ptr->tval == TV_CLOAK && o_ptr->sval == SV_MIMIC_CLOAK)
	{
		s32b mimic = find_random_mimic_shape(127, TRUE);
		o_ptr->pval2 = mimic;
	}
	else if (flags & TR_SPELL_CONTAIN)
	{
		o_ptr->pval2 = -1;
	}

	return TRUE;
}


bool_ artifact_scroll(void)
{
	bool_ okay = FALSE;

	/* Get an item */
	int item;
	if (!get_item(&item,
		      "Enchant which item? ",
		      "You have nothing to enchant.",
		      (USE_EQUIP | USE_INVEN | USE_FLOOR),
		      item_tester_hook_artifactable()))
	{
		return (FALSE);
	}

	/* Get the item */
	object_type *o_ptr = get_object(item);

	/* Description */
	char o_name[80];
	object_desc(o_name, o_ptr, FALSE, 0);

	/* Describe */
	msg_format("%s %s radiate%s a blinding light!",
	           ((item >= 0) ? "Your" : "The"), o_name,
	           ((o_ptr->number > 1) ? "" : "s"));

	if (artifact_p(o_ptr))
	{
		msg_format("The %s %s already %s!",
		           o_name, ((o_ptr->number > 1) ? "are" : "is"),
		           ((o_ptr->number > 1) ? "artifacts" : "an artifact"));
		okay = FALSE;
	}

	else if (o_ptr->name2)
	{
		msg_format("The %s %s already %s!",
		           o_name, ((o_ptr->number > 1) ? "are" : "is"),
		           ((o_ptr->number > 1) ? "ego items" : "an ego item"));
		okay = FALSE;
	}

	else
	{
		if (o_ptr->number > 1)
		{
			msg_print("Not enough enough energy to enchant more than one object!");
			msg_format("%d of your %s %s destroyed!", (o_ptr->number) - 1, o_name, (o_ptr->number > 2 ? "were" : "was"));
			o_ptr->number = 1;
		}
		okay = create_artifact(o_ptr, TRUE, TRUE);
	}

	/* Failure */
	if (!okay)
	{
		/* Flush */
		flush_on_failure();

		/* Message */
		msg_print("The enchantment failed.");
	}
	else
		o_ptr->found = OBJ_FOUND_SELFMADE;

	/* Something happened */
	return (TRUE);
}


