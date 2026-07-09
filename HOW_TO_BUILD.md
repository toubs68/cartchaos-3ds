# Cart Chaos 3D - Build & Install

A downhill shopping-cart chaos game for the Nintendo 3DS (homebrew).

> You do NOT need a PC or devkitPro installed. The `.3dsx` is built for free
> in GitHub's cloud via GitHub Actions.

## 1. Put the code on GitHub (one time)
You need a free GitHub account (you said you have one).

- Go to https://github.com/new
- Repository name: `cartchaos-3ds` (or anything you like)
- Keep it **Public** (private also works, but public keeps Actions free)
- Do **NOT** add a README/license (we already have them)
- Click **Create repository**

Then, on your phone, from the `cart-game` folder, run the included helper:

```sh
bash push.sh
```

It will ask for your GitHub username and the repository name you just created,
then push the code. When it prompts for a password, paste a
[GitHub Personal Access Token](https://github.com/settings/tokens)
(with `repo` scope) instead of your account password.

Manual equivalent (if you prefer):

```sh
git remote add origin https://github.com/<USER>/<REPO>.git
git branch -M main
git push -u origin main
```

## 2. Let GitHub build the .3dsx (automatic)
Pushing to `main` automatically starts the build:
- Open your repo on GitHub -> **Actions** tab
- You'll see "Build 3DS (cartchaos.3dsx)" running
- Wait ~1-2 minutes for the green check

## 3. Download the game
- In the finished Actions run, scroll to **Artifacts** -> `cartchaos-3dsx`
- Download and unzip it
- You get `cartchaos.3dsx` and `cartchaos.smdh`

(Optional) Tag a release to get a published download:
```sh
git tag v1.0.0
git push origin v1.0.0
```
This auto-creates a GitHub Release with the files attached.

## 4. Install on a homebrewed 3DS
- Copy `cartchaos.3dsx` **and** `cartchaos.smdh` into `SD:/3ds/CartChaos/`
- Launch via the Homebrew Launcher or directly from the home menu
- Requires a 3DS with boot9strap / Luma3DS (standard homebrew setup)

## 5. Controls
- **A / Circle-pad (left/right)**: push / steer the cart
- **B**: drink a booster (recover energy on flat sections)
- **Up/Down / touch**: lean (air control, pumping on descents)
- **Start**: restart after a crash

## Build it yourself (optional, needs a PC with devkitPro)
```sh
# on a PC with devkitPro (devkitARM + 3ds-dev) installed:
make            # -> build/cartchaos.3dsx
make cia        # -> cartchaos.cia  (installer, needs makerom)
```

## What the game is
Start by sprinting behind a supermarket cart, hop in, then ride pure inertia
and gravity down a cartoon city: roads, slopes, tunnels, bridges, traffic in
both directions, ramps, narrow passages, and boulder-chased cliff drops.
Drink pickups restore the energy needed to push on the flat bits. Exaggerated
crashes, speed lines, wheel rattle and screams included.
