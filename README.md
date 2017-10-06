# DotATranslator (v2)

This is DotA translator, a utility which translates DotA 2 text chat into your 
favourite language (and pops the results back into the chat pane). It works
in all game modes (*even in regular matchmaking!*).

## Screenshots

I don't have pictures playing with humans because I'm unable to exit 
Visual Studio (send help!).

![French, classy!](https://i.imgur.com/oQb0cxg.jpg)
![Russian, because why not!](https://i.imgur.com/SEJ2KxW.jpg)

I'm translating English to other languages in these examples but (obviously) it
works the other way around too.

## Languages

DotA translator supports all languages supported by Google Translate (100+).

If you want to translate everything that isn't English to English, this is what
your `Config.ini` should look like.

```
[Translation]
lang=en
```

If you want everything in Spanish instead, try:

```
[Translation]
lang=es
```

You might have seen, the `lang` option is a two-letter language code. Just
Google it if you don't remember yours.

## Installing

This only works on 64-bit Windows PCs with .NET 4.6.1 (pretty standard). Linux
and Win32 support is planned.

Head to the [Releases page](https://github.com/ur0/DotATranslator/releases) and
download the latest ZIP. Everything should be set up for you already.

## Updating

Because of the way this is written, I need to update it almost
every time there's an update for DotA 2. Specifically, there's a
teeny-tiny line over at `Injectee/dllmain.cpp` where I need to change a couple 
of letters.
Unfortunately, that can't be automated safely.

Just follow the installation procedure (hint: previous section) to update. Yep.
It's that simple.

I'm looking into automatic updates but don't expect it just yet. Sorryz.

## VAC

C'mon. This isn't a cheat. I've mailed Valve anyways
(just to be sure). There are legitimate utilities that do a lot more stuff
than this, in even shadier ways (looking at you, Discord Overlay). You can
be sure that this isn't gonna get you banned, promise!

## Contributing

Pull requests are welcome. I don't see any huge features this is missing, but
everyone loves new features. Right now, I'd love screenshots with actual
players because I don't get non-English speakers on Indian servers.

## Bugs

This is relatively stable. If you manage to find one somehow, just create an
issue here and I'll look into it ASAP.

If you just need help (regarding anything), 
I'm [/u/ur_0](https://reddit.com/ur_0) on Reddit.

## Linux and Win32 support

I'll probably implement Linux first. While it requires the entire thing to be
rewritten in C++, I'm pretty familiar with hooking functions there. Win32
support will need changes to the underlying library, EasyHook.

## Credits

Huge thanks to [/u/airoes](https://reddit.com/user/airoes) for letting me use
the r/dotatranslator subreddit and (slightly modified) name.

## Lawyer stuff

DotA Translator v2 is (c) Umang Raghuvanshi, 2017. The source code (excluding
any dependencies) is licensed to you under the MIT license, contents of which
are in the LICENSE.txt file.

Lawyers, let me know if this didn't sound like absolute nonsense.