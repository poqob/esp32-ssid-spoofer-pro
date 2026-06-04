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

  // Force block layout: stack cols vertically at full size
  await page.evaluate(() => {
    const s = document.createElement('style');
    s.textContent = `
      html,body{margin:0;padding:0;background:#1a1f2a}
      .wrap{display:block!important;padding:0!important;gap:0!important;overflow:visible!important}
      .col{display:block;width:1080px;height:1920px}
      .frame{width:1080px!important;height:1920px!important;transform:none!important;margin:0!important}
      .label{display:none!important}
    `;
    document.body.appendChild(s);
  });

  await new Promise(r => setTimeout(r, 300));

  const colRects = await page.evaluate(() => {
    return Array.from(document.querySelectorAll('.col')).map(el => {
      const r = el.getBoundingClientRect();
      return { x: r.x, y: r.y, w: r.width, h: r.height };
    });
  });

  const names = ['saturn_spoofer_dark', 'saturn_spoofer_light'];

  for (let i = 0; i < colRects.length; i++) {
    const name = names[i];
    const outPath = path.join(outDir, `${name}.png`);
    const { x, y, w, h } = colRects[i];

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
