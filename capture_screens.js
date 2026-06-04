const puppeteer = require('puppeteer');
const fs = require('fs');
const path = require('path');

(async () => {
  const outDir = path.join(__dirname, 'screenshots');
  if (!fs.existsSync(outDir)) fs.mkdirSync(outDir, { recursive: true });

  const browser = await puppeteer.launch({
    headless: true,
    args: ['--no-sandbox', '--disable-setuid-sandbox'],
  });

  const page = await browser.newPage();
  await page.setViewport({ width: 1080, height: 1920, deviceScaleFactor: 2 });

  const filePath = path.join(__dirname, 'mock-ui.html');
  await page.goto(`file://${filePath}`, { waitUntil: 'networkidle0', timeout: 30000 });

  await page.evaluate(() => document.fonts && document.fonts.ready);
  await new Promise(r => setTimeout(r, 500));

  // Remove scale transform so phones render at full size
  await page.evaluate(() => {
    const s = document.createElement('style');
    s.textContent = `
      .frame{transform:none!important;margin:0!important;width:1080px!important;height:1920px!important}
      .wrap{display:block!important;padding:0!important;gap:0!important}
      .col{display:block;width:1080px;height:1920px}
      body{background:transparent!important}
    `;
    document.body.appendChild(s);
  });
  await new Promise(r => setTimeout(r, 300));

  const phoneRects = await page.evaluate(() => {
    return Array.from(document.querySelectorAll('.phone')).map(el => {
      const r = el.getBoundingClientRect();
      return { x: r.x, y: r.y, w: r.width, h: r.height };
    });
  });

  const names = ['saturn_spoofer_dark', 'saturn_spoofer_light'];
  const pad = 40;

  for (let i = 0; i < phoneRects.length; i++) {
    const name = names[i];
    const outPath = path.join(outDir, `${name}.png`);
    let { x, y, w, h } = phoneRects[i];
    x -= pad; y -= pad; w += pad * 2; h += pad * 2;

    await page.screenshot({
      path: outPath,
      clip: { x: Math.round(x), y: Math.round(y), width: Math.round(w), height: Math.round(h) },
      type: 'png',
    });

    console.log(`  ✓ ${name}.png`);
  }

  await page.close();
  await browser.close();
  console.log('\nDone!');
})();
