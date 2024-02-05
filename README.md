# Nezplug (!tags.m3u version)

## About
This is a fork of the [Nezplug](https://github.com/AoiMoe/nezplug) winamp plugin to support tagging via a `!tags.m3u` file. The format itself is from [vgmstream](https://github.com/vgmstream/vgmstream). Support for `!tags.m3u` was coded from scratch as opposed from being based off of vgmstream's source code. [Nezplug supports the following formats:](https://www.vgmpf.com/Wiki/index.php?title=NEZ_Plug)
* AY
* GBS
* HES
* KSS
* NSF

## Example usage
`!tags.m3u`:
```m3u
# @album	Das Geheimnis der Happy Hippo-Insel
# @company	Kritzelkratz 3000, Infogrames
# @artist	Stello Doussis
# @year		2000-05-17
# @ripper	DevEd
# @tagger	DevEd
# @source	CGB-BHOD-GER.gbs

# %title	Title Screen
# %subtune	0
# %length	0:02:03.000
# %fade		0:00:10.000
01 Title Screen.m3u

# %title	Der Sonnenstrand
# %subtune	1
# %length	0:04:43.000
# %fade		0:00:10.000
02 Der Sonnenstrand.m3u

# %title	Durch den Dunkel-Dschungel
# %subtune	2
# %length	0:02:51.000
# %fade		0:00:10.000
03 Durch den Dunkel-Dschungel.m3u

# %title	In der Lauschigen Lagune
# %subtune	3
# %length	0:05:40.000
# %fade		0:00:10.000
04 In der Lauschigen Lagune.m3u

# %title	Im Bröselbach
# %subtune	4
# %length	0:05:20.000
# %fade		0:00:10.000
05 Im Broselbach.m3u

# %title	Auf den Wirren Wetterwarte
# %subtune	5
# %length	0:07:35.000
# %fade		0:00:10.000
06 Auf der Wirren Wetterwarte.m3u
```

`01 Title Screen.m3u`:

```m3u
01 Title Screen.m3u.gbs
```

## Issues
I had to imploy a neccessary hack for the winamp plugin: m3u track file contents must have their own filename with the extension `.m3u.xxx` (where `xxx` is the extension of the music format). This is so winamp can determine what plugin to use.

## But why?
This fork addresses a long standing tech debt item that has plagued old sequenced rips for over the last 20 years. While NezPlug currently supports M3U tagging, the format is quite ugly:

`CGB-BHOD-GER.m3u`:
```m3u
# @TITLE       Das Geheimnis der Happy Hippo-Insel
# @ARTIST      Kritzelkratz 3000, Infogrames
# @COMPOSER    Stello Doussis
# @DATE        2000-05-17
# @RIPPER      DevEd
# @TAGGER      DevEd

CGB-BHOD-GER.gbs::GBS,0,Title Screen,2:03,,10
CGB-BHOD-GER.gbs::GBS,1,Der Sonnenstrand,4:43,,10
CGB-BHOD-GER.gbs::GBS,2,Durch den Dunkel-Dschungel,2:51,,10
CGB-BHOD-GER.gbs::GBS,3,In der Lauschingen Lagune,5:40,,10
CGB-BHOD-GER.gbs::GBS,4,Im Bröselbach,5:20,,10
CGB-BHOD-GER.gbs::GBS,5,Auf den Wirren Wetterwarte,7:35,,10
```

`01 Title Screen.m3u`:
```m3u
CGB-BHOD-GER.gbs::GBS,0,Title Screen - Stello Doussis - Das Geheimnis der Happy Hippo-Insel - ©2000 Kritzelkratz 3000\, Infogrames,2:03,,10
```

Other alternatives are:
* [NSFE](https://www.vgmpf.com/Wiki/index.php?title=NSFE)
    * Track time and tags are embedded into the binary
    * You still have to deal with the problem of subtunes
    * Only specific to NSFs
* Trimming NSFs, GBSes, etc so each song is a file
    * Currently, this cannot be done at a widescale

The solution this fork provides is highly scalable and offers an easy way to read and adjust tags.
