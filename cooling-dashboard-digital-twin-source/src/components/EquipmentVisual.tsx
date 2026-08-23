import {
  Fan,
  Gauge,
  Snowflake,
  Thermometer,
  Waves,
  Zap,
} from 'lucide-react'


type EquipmentState =
  | 'normal'
  | 'warning'
  | 'alarm'
  | 'offline'


interface EquipmentVisualProps {
  name?: string
  state: EquipmentState
  running?: boolean
}


const stateStyles = {
  normal: {
    body: 'from-cyan-400 to-blue-600',
    glow: 'shadow-cyan-200/70',
    indicator: 'bg-emerald-400',
  },
  warning: {
    body: 'from-amber-300 to-orange-500',
    glow: 'shadow-amber-200/70',
    indicator: 'bg-amber-400',
  },
  alarm: {
    body: 'from-red-400 to-rose-700',
    glow: 'shadow-red-200/70',
    indicator: 'bg-red-500 animate-pulse',
  },
  offline: {
    body: 'from-slate-300 to-slate-500',
    glow: 'shadow-slate-200/70',
    indicator: 'bg-slate-400',
  },
}


function equipmentType(name?: string) {
  const normalized = name?.toUpperCase() ?? ''

  if (normalized.includes('FAN')) return 'fan'
  if (normalized.includes('PRESS')) return 'pressure'
  if (normalized.includes('TEMP')) return 'temperature'
  if (normalized.includes('LEVEL')) return 'level'
  if (normalized.includes('POWER') || normalized.includes('CURRENT')) return 'power'
  return 'cooling'
}


function EquipmentVisual({
  name,
  state,
  running = false,
}: EquipmentVisualProps) {
  const styles = stateStyles[state]
  const type = equipmentType(name)
  const Icon =
    type === 'fan'
      ? Fan
      : type === 'pressure'
        ? Gauge
        : type === 'temperature'
          ? Thermometer
          : type === 'level'
            ? Waves
            : type === 'power'
              ? Zap
              : Snowflake

  return (
    <div className="relative flex h-28 items-center justify-center [perspective:500px]">
      <div className={`absolute bottom-2 h-5 w-24 rounded-[50%] bg-slate-300/60 blur-md ${styles.glow}`} />

      <div className="relative [transform:rotateX(9deg)_rotateY(-13deg)]">
        <div className={`relative flex h-20 w-24 items-center justify-center rounded-2xl border border-white/70 bg-gradient-to-br ${styles.body} shadow-xl ${styles.glow}`}>
          <div className="absolute inset-x-2 top-2 h-2 rounded-full bg-white/35" />
          <div className="absolute -right-2 bottom-3 top-4 w-3 rounded-r-lg bg-slate-700/30" />
          <Icon
            size={38}
            strokeWidth={1.7}
            className={[
              'text-white drop-shadow-md',
              type === 'fan' && running
                ? 'animate-spin'
                : '',
            ].join(' ')}
          />
          <span className={`absolute bottom-2 right-2 size-2.5 rounded-full border border-white/80 ${styles.indicator}`} />
        </div>
        <div className="mx-auto h-2 w-16 rounded-b-lg bg-slate-600/70" />
      </div>
    </div>
  )
}


export type { EquipmentState }
export default EquipmentVisual
