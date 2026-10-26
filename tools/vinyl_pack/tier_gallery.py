"""Render a manually reviewed tier manifest; never change mod assets or saves."""
import argparse
import collections
import csv
import hashlib
import html
import json
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('release', type=Path)
    p.add_argument('--review', type=Path, default=Path(__file__).with_name('visual_tiers_v2.json'))
    a = p.parse_args()
    review = json.loads(a.review.read_text())
    entries = review['entries']
    catalogue = json.loads((a.release / 'underground-vinyls/catalogue.json').read_text())
    assert len({r['id'] for r in entries}) == len(entries)
    assert {r['id'] for r in entries} == {r['id'] for r in catalogue}, 'Incomplete review'
    gallery = a.release / 'gallery'
    report = a.release / 'tier-review'
    report.mkdir(exist_ok=True)
    counts = collections.Counter(r['tier'] for r in entries)
    cards = []
    evidence = {}
    for r in entries:
        assert r['tier'] in 'ABCD' and len(r['tier']) == 1
        assert r['recommendation'] == ('keep' if r['tier'] in 'AB' else 'remove')
        assert r['image'] == r['id'] + '.jpg'
        image = gallery / r['image']
        evidence[r['id']] = hashlib.sha256(image.read_bytes()).hexdigest()
        e = {k: html.escape(str(v), quote=True) for k, v in r.items()}
        cards.append(
            f'<article data-tier="{e["tier"]}" id="{e["id"]}">'
            f'<header><b class="tier {e["tier"]}">{e["tier"]}</b>'
            f'<div><h2>{e["car"]} · {e["label"]}</h2>'
            f'<small>{e["id"]} · {e["pack"].upper()}</small></div></header>'
            f'<a href="{e["image"]}" target="_blank"><img loading="lazy" '
            f'src="{e["image"]}" alt="Tres vistas de {e["label"]}"></a>'
            f'<p>{e["reason"]}</p><footer>'
            + ('Conservar' if r['tier'] in 'AB' else 'Retirar en la siguiente selección')
            + '</footer></article>')
    definitions = ''.join(f'<li><b>{t}</b> — {html.escape(review["criteria"][t])}</li>' for t in 'ABCD')
    options = ''.join(f'<option value="{t}">{t} · {counts[t]}</option>' for t in 'ABCD')
    page = '''<!doctype html><html lang="es"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OpenJuice · Revisión de vinilos v2</title>
<style>
:root{color-scheme:dark}*{box-sizing:border-box}body{margin:0;background:#101318;color:#e9edf2;font:16px/1.5 system-ui}
main{max-width:1180px;margin:auto;padding:32px 20px}h1{font-size:32px;margin-bottom:10px}h2{font-size:18px;margin:0}
small,footer{color:#aeb8c6}.intro{max-width:900px}.controls{position:sticky;top:0;background:#101318f5;padding:15px 0;z-index:1;display:flex;gap:10px;flex-wrap:wrap}
input,select{font:inherit;padding:10px;background:#232933;color:#fff;border:1px solid #566170;border-radius:6px}input{flex:1;min-width:200px}
article{border:1px solid #37404d;background:#191e26;margin:24px 0;border-radius:8px;overflow:hidden}article header{display:flex;align-items:center;gap:14px;padding:16px}
.tier{display:grid;place-items:center;width:42px;height:42px;border-radius:5px;color:#101318;font-size:24px}.A{background:#79ddb5}.B{background:#8dbfff}.C{background:#efc56f}.D{background:#f3989e}
img{display:block;width:100%;height:auto}article p{padding:0 16px}footer{padding:0 16px 16px}a{color:#a7cdff}li{margin:6px 0}[hidden]{display:none!important}
</style><main><h1>Revisión visual · Underground Vinyls v2</h1>
<div class="intro"><p>241 opciones revisadas individualmente, tres vistas por opción. A/B se conservan; C/D se proponen para retirar. <strong>No se ha eliminado ningún vinilo.</strong></p>
<p>Clasificación del resultado visible sobre carrocería de fábrica y la pintura de estas capturas. No es una certificación de kits ni de funcionamiento en carrera. Las opciones con poco contraste se juzgan tal como aparecen aquí.</p>
<ul>''' + definitions + '''</ul><p><a href="../tier-review/ratings.csv">CSV con motivos</a> · <a href="../tier-review/keep-ids.txt">IDs A/B</a> · <a href="../tier-review/remove-ids.txt">IDs C/D</a></p></div>
<div class="controls"><select id="tier" aria-label="Filtrar tier"><option value="all">Todos · 241</option><option value="keep">Conservar · A/B</option><option value="remove">Retirar · C/D</option>''' + options + '''</select>
<input id="search" type="search" placeholder="Buscar coche, diseño, ID o motivo" aria-label="Buscar"><span id="count" aria-live="polite"></span></div>''' + ''.join(cards) + '''
<script>
const tier=document.getElementById('tier'), search=document.getElementById('search');
function filter(){let n=0;for(const card of document.querySelectorAll('article')){
const t=card.dataset.tier, group=tier.value;
const match=group==='all'||group===t||(group==='keep'&&'AB'.includes(t))||(group==='remove'&&'CD'.includes(t));
card.hidden=!(match&&card.textContent.toLowerCase().includes(search.value.toLowerCase()));if(!card.hidden)n++;}
document.getElementById('count').textContent=n+' opciones';}
tier.addEventListener('change',filter);search.addEventListener('input',filter);filter();
</script></main></html>'''
    (gallery / 'index.html').write_text(page)
    (report / 'review.json').write_text(json.dumps(review, ensure_ascii=False, indent=2) + '\n')
    (report / 'image-sha256.json').write_text(json.dumps(evidence, indent=2) + '\n')
    with (report / 'ratings.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(entries[0]))
        writer.writeheader()
        writer.writerows(entries)
    for name, tiers in [('keep', 'AB'), ('remove', 'CD')]:
        (report / (name + '-ids.txt')).write_text(''.join(r['id'] + '\n' for r in entries if r['tier'] in tiers))
    assert page.count('<article ') == len(entries)
    print(dict(sorted(counts.items())))
    print(gallery / 'index.html')


if __name__ == '__main__':
    main()
