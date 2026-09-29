/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef INSTALLER_MANIFEST_H
#define INSTALLER_MANIFEST_H

// The canonical Tyrian 2000 runtime set: the size and POSIX cksum CRC of every
// file the engine reads from the verified archive.  Metadata only.  Generated
// from test/regress-2000/data-manifest.txt, and tools/check_installer.sh
// fails if the two ever differ.

typedef struct
{
	unsigned long size;
	unsigned long crc;
	const char *name;
} InstallerManifestEntry;

static const InstallerManifestEntry installerCanonicalManifest[] =
{
	{ 35780, 1209265539UL, "cubetxt1.dat" },
	{ 8109, 2712896192UL, "cubetxt2.dat" },
	{ 12277, 2914551291UL, "cubetxt3.dat" },
	{ 61763, 2766464770UL, "cubetxt4.dat" },
	{ 7024, 2416136283UL, "cubetxt5.dat" },
	{ 2745, 625760848UL, "demo.1" },
	{ 2193, 3214433957UL, "demo.2" },
	{ 1974, 623644273UL, "demo.3" },
	{ 699, 1914765014UL, "demo.4" },
	{ 1122, 2152709229UL, "demo.5" },
	{ 119552, 3388891273UL, "estpa.shp" },
	{ 115338, 1085736327UL, "estsc.shp" },
	{ 9600, 2858812964UL, "levels1.dat" },
	{ 5534, 1788176354UL, "levels2.dat" },
	{ 6052, 1789566389UL, "levels3.dat" },
	{ 11116, 3983064838UL, "levels4.dat" },
	{ 4257, 3415369915UL, "levels5.dat" },
	{ 153482, 842346037UL, "music.mus" },
	{ 17750, 2070673289UL, "newsh#.shp" },
	{ 18016, 86096781UL, "newsh$.shp" },
	{ 40244, 1307984159UL, "newsh%.shp" },
	{ 35108, 2957881770UL, "newsh'.shp" },
	{ 32060, 1579300666UL, "newsh(.shp" },
	{ 14096, 3328586459UL, "newsh0.shp" },
	{ 29941, 2874864033UL, "newsh1.shp" },
	{ 35236, 1993929893UL, "newsh2.shp" },
	{ 37130, 2811399401UL, "newsh3.shp" },
	{ 27109, 1902489147UL, "newsh4.shp" },
	{ 35810, 2529306122UL, "newsh5.shp" },
	{ 17810, 3585975344UL, "newsh6.shp" },
	{ 44079, 2593278867UL, "newsh7.shp" },
	{ 35152, 975069015UL, "newsh8.shp" },
	{ 38831, 3600755471UL, "newsh9.shp" },
	{ 16498, 2621933012UL, "newsh@.shp" },
	{ 34888, 711303051UL, "newsh^.shp" },
	{ 25622, 2511252582UL, "newsha.shp" },
	{ 39025, 2458773564UL, "newshb.shp" },
	{ 40714, 117528338UL, "newshc.shp" },
	{ 42012, 1017427299UL, "newshd.shp" },
	{ 33563, 3448371693UL, "newshe.shp" },
	{ 33543, 3376643067UL, "newshf.shp" },
	{ 36643, 3829556607UL, "newshg.shp" },
	{ 46639, 4102196915UL, "newshh.shp" },
	{ 30860, 2722307320UL, "newshi.shp" },
	{ 43999, 3409272313UL, "newshj.shp" },
	{ 23085, 301199774UL, "newshk.shp" },
	{ 27587, 4289035018UL, "newshl.shp" },
	{ 42126, 3407189224UL, "newshm.shp" },
	{ 41201, 1292696811UL, "newshn.shp" },
	{ 24478, 3002203261UL, "newsho.shp" },
	{ 46204, 1454869397UL, "newshp.shp" },
	{ 28249, 3612603756UL, "newshr.shp" },
	{ 46566, 2487630938UL, "newshs.shp" },
	{ 31830, 1799186985UL, "newsht.shp" },
	{ 35657, 1233745291UL, "newshu.shp" },
	{ 24103, 35713433UL, "newshv.shp" },
	{ 14929, 3947938427UL, "newsh~.shp" },
	{ 18432, 350947577UL, "palette.dat" },
	{ 118720, 2717272862UL, "shapes).dat" },
	{ 172480, 1648817559UL, "shapesw.dat" },
	{ 217504, 767763841UL, "shapesx.dat" },
	{ 206752, 1717005239UL, "shapesy.dat" },
	{ 318976, 959231650UL, "shapesz.dat" },
	{ 3315848, 3988034873UL, "tyrend.anm" },
	{ 1078, 4139414299UL, "tyrian.cdt" },
	{ 295069, 1136020222UL, "tyrian.hdt" },
	{ 367161, 501130314UL, "tyrian.pic" },
	{ 505983, 3647119929UL, "tyrian.shp" },
	{ 271689, 3556103808UL, "tyrian.snd" },
	{ 538856, 743471398UL, "tyrian1.lvl" },
	{ 381187, 2271820750UL, "tyrian2.lvl" },
	{ 393938, 478873825UL, "tyrian3.lvl" },
	{ 937303, 106289611UL, "tyrian4.lvl" },
	{ 521137, 2948367722UL, "tyrian5.lvl" },
	{ 516757, 447103242UL, "tyrianc.shp" },
	{ 27674, 491299863UL, "user1.shp" },
	{ 27674, 84075211UL, "user2.shp" },
	{ 132767, 1661905447UL, "voices.snd" },
	{ 188275, 2600234191UL, "voicesc.snd" },
};

#endif // INSTALLER_MANIFEST_H
