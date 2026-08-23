import type { Alarm, TelemetrySignal } from '../types/api'


interface CoolingSystemSceneProps {
  online: boolean
  signals: TelemetrySignal[]
  alarms: Alarm[]
}


function componentState(
  signal: TelemetrySignal | undefined,
  alarms: Alarm[],
  online: boolean,
) {
  if (!online) return 'offline'

  const alarm = alarms.find(
    (item) =>
      item.source_signal === signal?.name,
  )

  if (alarm?.severity?.toUpperCase() === 'CRITICAL'
      || alarm?.severity?.toUpperCase() === 'HIGH') {
    return 'alarm'
  }

  if (alarm || signal?.quality?.toLowerCase() === 'stale') {
    return 'warning'
  }

  if (!signal || signal.quality?.toLowerCase() === 'unavailable') {
    return 'offline'
  }

  return 'normal'
}


const colors = {
  normal: '#0ea5e9',
  warning: '#f59e0b',
  alarm: '#ef4444',
  offline: '#94a3b8',
}


function CoolingSystemScene({
  online,
  signals,
  alarms,
}: CoolingSystemSceneProps) {
  const temperature = signals.find((signal) =>
    signal.name?.includes('TEMP'),
  )
  const pressure = signals.find((signal) =>
    signal.name?.includes('PRESS'),
  )
  const fan = signals.find((signal) =>
    signal.name?.includes('FAN'),
  )
  const compressor = signals.find((signal) =>
    signal.name?.includes('COMPRESSOR'),
  )

  const temperatureState = componentState(temperature, alarms, online)
  const pressureState = componentState(pressure, alarms, online)
  const fanState = componentState(fan, alarms, online)
  const compressorState = componentState(compressor, alarms, online)

  const fanRunning =
    fan?.value === true
    || fan?.value === 1
    || String(fan?.value).toLowerCase() === 'running'

  return (
    <div className="relative overflow-hidden rounded-3xl bg-gradient-to-br from-sky-50 via-white to-blue-50 p-4 sm:p-6">
      <div className="absolute -right-20 -top-20 size-64 rounded-full bg-cyan-200/25 blur-3xl" />
      <svg
        viewBox="0 0 820 420"
        role="img"
        aria-label="Interactive isometric cooling system overview"
        className="relative w-full"
      >
        <defs>
          <linearGradient id="floor" x1="0" y1="0" x2="1" y2="1">
            <stop offset="0" stopColor="#e0f2fe" />
            <stop offset="1" stopColor="#f8fafc" />
          </linearGradient>
          <filter id="shadow" x="-30%" y="-30%" width="160%" height="160%">
            <feDropShadow dx="0" dy="10" stdDeviation="8" floodColor="#0f172a" floodOpacity="0.14" />
          </filter>
        </defs>

        <path d="M84 308 378 138 744 264 450 404Z" fill="url(#floor)" stroke="#bae6fd" strokeWidth="2" />

        <path d="M262 266 C330 230 374 227 439 251" fill="none" stroke="#38bdf8" strokeWidth="13" strokeLinecap="round" />
        <path d="M479 252 C548 224 594 225 650 260" fill="none" stroke="#fb7185" strokeWidth="13" strokeLinecap="round" />
        <path d="M652 287 C569 337 464 346 365 309" fill="none" stroke="#60a5fa" strokeWidth="10" strokeLinecap="round" strokeDasharray="5 8" />

        <g transform="translate(115 208)" filter="url(#shadow)">
          <path d="M0 39 91 0 177 35 87 78Z" fill="#cbd5e1" />
          <path d="M0 39v91l87 43V78Z" fill="#e2e8f0" />
          <path d="M87 78v95l90-50V35Z" fill="#f8fafc" />
          <path d="M21 57v58l47 23V80Z" fill={colors[temperatureState]} opacity="0.86" />
          <path d="M110 76v62l45-25V52Z" fill="#dbeafe" stroke="#93c5fd" />
          <text x="75" y="198" textAnchor="middle" fill="#334155" fontSize="15" fontWeight="700">COLD ROOM</text>
        </g>

        <g transform="translate(334 222)" filter="url(#shadow)">
          <path d="M0 49 74 13 147 45 72 84Z" fill="#94a3b8" />
          <rect x="18" y="31" width="108" height="72" rx="18" fill={colors[compressorState]} />
          <ellipse cx="72" cy="31" rx="54" ry="23" fill="#e0f2fe" stroke={colors[compressorState]} strokeWidth="5" />
          <circle cx="72" cy="31" r="12" fill="#fff" opacity="0.75" />
          <path d="M37 102v25M108 102v25" stroke="#475569" strokeWidth="8" />
          <text x="73" y="154" textAnchor="middle" fill="#334155" fontSize="15" fontWeight="700">COMPRESSOR</text>
        </g>

        <g transform="translate(556 180)" filter="url(#shadow)">
          <path d="M0 42 83 6 165 42 82 79Z" fill="#cbd5e1" />
          <path d="M12 44v85l70 35V79Z" fill={colors[fanState]} opacity="0.88" />
          <path d="M82 79v85l70-39V44Z" fill="#e2e8f0" />
          {[44, 89, 134].map((x) => (
            <g key={x} transform={`translate(${x} 49)`}>
              <circle r="21" fill="#fff" opacity="0.82" stroke={colors[fanState]} strokeWidth="4" />
              <g className={fanRunning ? 'origin-center animate-spin' : ''}>
                <path d="M0 0C2-18 13-19 15-9C17 1 9 7 0 0Z" fill={colors[fanState]} />
                <path d="M0 0C18 2 19 13 9 15C-1 17-7 9 0 0Z" fill={colors[fanState]} />
                <path d="M0 0C-2 18-13 19-15 9C-17-1-9-7 0 0Z" fill={colors[fanState]} />
              </g>
            </g>
          ))}
          <text x="82" y="190" textAnchor="middle" fill="#334155" fontSize="15" fontWeight="700">CONDENSER</text>
        </g>

        <g transform="translate(426 135)">
          <circle r="39" fill="#fff" stroke={colors[pressureState]} strokeWidth="7" filter="url(#shadow)" />
          <path d="M0 0 18-18" stroke={colors[pressureState]} strokeWidth="6" strokeLinecap="round" />
          <circle r="6" fill="#334155" />
          <text y="61" textAnchor="middle" fill="#475569" fontSize="13" fontWeight="700">PRESSURE</text>
        </g>

        <g transform="translate(172 150)">
          <rect x="-16" y="-35" width="32" height="57" rx="16" fill="#fff" stroke={colors[temperatureState]} strokeWidth="6" />
          <circle cy="20" r="20" fill={colors[temperatureState]} />
          <rect x="-5" y="-25" width="10" height="47" rx="5" fill={colors[temperatureState]} />
          <text y="62" textAnchor="middle" fill="#475569" fontSize="13" fontWeight="700">TEMPERATURE</text>
        </g>
      </svg>

      <div className="grid gap-2 text-xs sm:grid-cols-4">
        {[
          ['Cold Room', temperatureState],
          ['Compressor', compressorState],
          ['Condenser', fanState],
          ['Pressure Line', pressureState],
        ].map(([label, state]) => (
          <div key={label} className="flex items-center gap-2 rounded-xl border border-slate-200 bg-white/80 px-3 py-2 text-slate-600 shadow-sm backdrop-blur">
            <span className="size-2.5 rounded-full" style={{ backgroundColor: colors[state as keyof typeof colors] }} />
            {label}
          </div>
        ))}
      </div>
    </div>
  )
}


export default CoolingSystemScene
