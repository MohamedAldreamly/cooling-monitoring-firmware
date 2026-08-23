import { useQuery } from '@tanstack/react-query'
import type { LucideIcon } from 'lucide-react'
import {
  Activity,
  AlertTriangle,
  BellRing,
  Boxes,
  CircleCheck,
  CircleX,
  Clock3,
  Cpu,
  RefreshCw,
  Snowflake,
  Wifi,
} from 'lucide-react'
import { Link } from 'react-router-dom'
import {
  lazy,
  Suspense,
} from 'react'
import {
  CartesianGrid,
  Line,
  LineChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'

import {
  getActiveAlarms,
  getDashboardSummary,
  getDeviceAlarms,
  getDevices,
  getDeviceTelemetry,
} from '../api/dashboardApi'
import type {
  Alarm,
  TelemetryRecord,
  TelemetrySignal,
} from '../types/api'
import EquipmentVisual from '../components/EquipmentVisual'
import type { EquipmentState } from '../components/EquipmentVisual'


const DigitalTwin3D = lazy(
  () => import('../components/DigitalTwin3D'),
)


interface SummaryCardProps {
  title: string
  value: number
  icon: LucideIcon
  color: 'cyan' | 'green' | 'red' | 'amber'
}


const colorStyles = {
  cyan: {
    container: 'border-cyan-500/20 bg-cyan-500/5',
    icon: 'bg-cyan-500/15 text-cyan-400',
  },
  green: {
    container: 'border-emerald-500/20 bg-emerald-500/5',
    icon: 'bg-emerald-500/15 text-emerald-400',
  },
  red: {
    container: 'border-red-500/20 bg-red-500/5',
    icon: 'bg-red-500/15 text-red-400',
  },
  amber: {
    container: 'border-amber-500/20 bg-amber-500/5',
    icon: 'bg-amber-500/15 text-amber-400',
  },
}


function SummaryCard({
  title,
  value,
  icon: Icon,
  color,
}: SummaryCardProps) {
  const styles = colorStyles[color]

  return (
    <article
      className={[
        'rounded-2xl border p-5 shadow-lg shadow-black/5',
        styles.container,
      ].join(' ')}
    >
      <div className="flex items-start justify-between">
        <div>
          <p className="text-sm text-slate-400">
            {title}
          </p>
          <p className="mt-3 text-3xl font-bold text-white">
            {value}
          </p>
        </div>

        <div
          className={[
            'flex size-11 items-center justify-center rounded-xl',
            styles.icon,
          ].join(' ')}
        >
          <Icon size={22} />
        </div>
      </div>
    </article>
  )
}


function severityClass(severity?: string) {
  switch (severity?.toUpperCase()) {
    case 'CRITICAL':
      return 'bg-red-500/15 text-red-400'
    case 'HIGH':
      return 'bg-orange-500/15 text-orange-400'
    case 'WARNING':
      return 'bg-amber-500/15 text-amber-400'
    default:
      return 'bg-slate-700 text-slate-300'
  }
}


function signalLabel(name?: string) {
  if (!name) {
    return 'Unknown Signal'
  }

  return name
    .replace(/_\d+$/, '')
    .replaceAll('_', ' ')
    .toLowerCase()
    .replace(/\b\w/g, (letter) =>
      letter.toUpperCase(),
    )
}


function displaySignalValue(signal?: TelemetrySignal) {
  if (
    signal?.value === null
    || signal?.value === undefined
  ) {
    return 'Unavailable'
  }

  const value =
    typeof signal.value === 'number'
      ? Number.isInteger(signal.value)
        ? signal.value
        : signal.value.toFixed(1)
      : String(signal.value)

  return `${value}${signal.unit ? ` ${signal.unit}` : ''}`
}


function signalState(
  signal: TelemetrySignal,
  alarms: Alarm[],
  online: boolean,
): EquipmentState {
  if (!online) return 'offline'

  const alarm = alarms.find(
    (item) =>
      item.source_signal === signal.name,
  )

  if (
    alarm?.severity?.toUpperCase() === 'CRITICAL'
    || alarm?.severity?.toUpperCase() === 'HIGH'
  ) {
    return 'alarm'
  }

  const quality = signal.quality?.toLowerCase()

  if (alarm || quality === 'stale') {
    return 'warning'
  }

  if (
    quality === 'fault'
    || quality === 'unavailable'
    || signal.value === null
    || signal.value === undefined
  ) {
    return 'offline'
  }

  return 'normal'
}


function latestRecord(
  records: TelemetryRecord[],
) {
  return [...records].sort(
    (left, right) =>
      (right.received_at_ms ?? 0)
      - (left.received_at_ms ?? 0),
  )[0]
}


function DashboardPage() {
  const summaryQuery = useQuery({
    queryKey: ['dashboard-summary'],
    queryFn: getDashboardSummary,
    refetchInterval: 10000,
  })

  const devicesQuery = useQuery({
    queryKey: ['devices'],
    queryFn: getDevices,
    refetchInterval: 15000,
  })

  const alarmsQuery = useQuery({
    queryKey: ['active-alarms'],
    queryFn: getActiveAlarms,
    refetchInterval: 10000,
  })

  const devices =
    devicesQuery.data?.devices ?? []
  const primaryDevice = devices[0]

  const telemetryQuery = useQuery({
    queryKey: [
      'dashboard-telemetry',
      primaryDevice?.device_id,
    ],
    queryFn: () =>
      getDeviceTelemetry(
        primaryDevice?.device_id ?? '',
        60,
      ),
    enabled: Boolean(primaryDevice?.device_id),
    refetchInterval: 10000,
  })

  const alarmHistoryQuery = useQuery({
    queryKey: [
      'dashboard-alarm-history',
      primaryDevice?.device_id,
    ],
    queryFn: () =>
      getDeviceAlarms(
        primaryDevice?.device_id ?? '',
        50,
      ),
    enabled: Boolean(primaryDevice?.device_id),
    refetchInterval: 10000,
  })

  const telemetry =
    telemetryQuery.data?.telemetry ?? []
  const currentTelemetry =
    latestRecord(telemetry)
  const currentSignals =
    currentTelemetry?.signals ?? []

  const numericSignalNames = Array.from(
    new Set(
      telemetry.flatMap((record) =>
        record.signals
          ?.filter((signal) =>
            typeof signal.value === 'number',
          )
          .map((signal) => signal.name)
          .filter(
            (name): name is string =>
              Boolean(name),
          )
        ?? [],
      ),
    ),
  )

  const trendSignalName =
    numericSignalNames[0] ?? ''

  const trendSignalUnit = telemetry
    .flatMap((record) => record.signals ?? [])
    .find((signal) =>
      signal.name === trendSignalName,
    )?.unit ?? ''

  const trendData = telemetry
    .map((record) => {
      const signal = record.signals?.find(
        (item) =>
          item.name === trendSignalName,
      )

      return {
        time: record.received_at_ms
          ? new Date(
              record.received_at_ms,
            ).toLocaleTimeString([], {
              hour: '2-digit',
              minute: '2-digit',
            })
          : '',
        value:
          typeof signal?.value === 'number'
            ? signal.value
            : null,
      }
    })
    .filter((point) => point.value !== null)
    .reverse()

  const activeAlarms =
    alarmsQuery.data?.alarms ?? []
  const alarmHistory =
    alarmHistoryQuery.data?.alarms ?? []
  const summary = summaryQuery.data

  const isLoading =
    summaryQuery.isLoading
    || devicesQuery.isLoading
    || alarmsQuery.isLoading

  const hasError =
    summaryQuery.isError
    || devicesQuery.isError
    || alarmsQuery.isError

  const isRefreshing =
    summaryQuery.isFetching
    || devicesQuery.isFetching
    || alarmsQuery.isFetching
    || alarmHistoryQuery.isFetching
    || telemetryQuery.isFetching

  const primaryOnline =
    primaryDevice?.status?.toLowerCase()
    === 'online'

  function refreshDashboard() {
    void Promise.all([
      summaryQuery.refetch(),
      devicesQuery.refetch(),
      alarmsQuery.refetch(),
      telemetryQuery.refetch(),
    ])
  }

  if (isLoading) {
    return (
      <div className="flex min-h-80 items-center justify-center">
        <div className="text-center">
          <RefreshCw
            className="mx-auto animate-spin text-cyan-400"
            size={32}
          />
          <p className="mt-4 text-slate-400">
            Loading cooling system data...
          </p>
        </div>
      </div>
    )
  }

  if (hasError || !summary) {
    return (
      <div className="rounded-2xl border border-red-500/20 bg-red-500/5 p-8">
        <h2 className="text-xl font-semibold text-red-400">
          Unable to load dashboard
        </h2>
        <p className="mt-2 text-slate-400">
          Check the API URL, CORS configuration and internet connection.
        </p>
        <button
          type="button"
          onClick={refreshDashboard}
          className="mt-5 rounded-xl bg-red-500 px-5 py-2.5 font-medium text-white hover:bg-red-400"
        >
          Try again
        </button>
      </div>
    )
  }

  return (
    <section>
      <div className="flex flex-col justify-between gap-4 md:flex-row md:items-center">
        <div>
          <p className="text-xs font-semibold uppercase tracking-[0.22em] text-cyan-400">
            Industrial refrigeration monitoring
          </p>
          <h1 className="mt-2 text-3xl font-bold text-white">
            Cooling System Dashboard
          </h1>
          <p className="mt-2 text-slate-400">
            Live plant overview, equipment health and active alarms.
          </p>
        </div>

        <button
          type="button"
          onClick={refreshDashboard}
          disabled={isRefreshing}
          className="flex items-center justify-center gap-2 rounded-xl border border-slate-700 bg-slate-900 px-4 py-2.5 text-sm text-slate-300 transition hover:border-cyan-500/40 hover:text-cyan-300 disabled:opacity-50"
        >
          <RefreshCw
            size={17}
            className={
              isRefreshing
                ? 'animate-spin'
                : ''
            }
          />
          Refresh
        </button>
      </div>

      <article className="relative mt-7 min-h-[330px] overflow-hidden rounded-3xl border border-slate-200 bg-white shadow-xl shadow-slate-200/60">
        <img
          src="/cooling-system-hero.png"
          alt="Industrial refrigeration and cooling equipment"
          className="absolute inset-y-0 right-0 h-full w-full object-cover md:w-[68%]"
        />
        <div className="absolute inset-0 bg-gradient-to-r from-white via-white/95 to-white/10" />
        <div className="absolute inset-0 bg-gradient-to-t from-white/45 via-transparent to-white/10" />

        <div className="relative z-10 flex min-h-[330px] max-w-2xl flex-col justify-center p-7 sm:p-10">
          <div className="flex size-12 items-center justify-center rounded-2xl border border-cyan-200 bg-cyan-50 text-cyan-600">
            <Snowflake size={26} />
          </div>

          <p className="mt-5 text-sm font-medium uppercase tracking-[0.2em] text-cyan-600">
            Main cooling plant
          </p>
          <h2 className="mt-2 text-3xl font-bold text-slate-900 sm:text-4xl">
            {primaryDevice?.device_id
              ?? 'No device registered'}
          </h2>
          <p className="mt-3 max-w-xl text-sm leading-6 text-slate-600 sm:text-base">
            Real-time visibility into refrigeration signals, cloud connectivity, alarm activity and device health.
          </p>

          <div className="mt-6 flex flex-wrap gap-3">
            <span
              className={[
                'inline-flex items-center gap-2 rounded-xl border px-4 py-2 text-sm font-semibold backdrop-blur',
                primaryOnline
                  ? 'border-emerald-200 bg-emerald-50 text-emerald-700'
                  : 'border-red-200 bg-red-50 text-red-700',
              ].join(' ')}
            >
              <span
                className={[
                  'size-2 rounded-full',
                  primaryOnline
                    ? 'bg-emerald-400'
                    : 'bg-red-400',
                ].join(' ')}
              />
              {primaryOnline ? 'System Online' : 'System Offline'}
            </span>

            <span className="inline-flex items-center gap-2 rounded-xl border border-slate-200 bg-white/85 px-4 py-2 text-sm text-slate-600 backdrop-blur">
              <Cpu size={16} />
              Firmware {primaryDevice?.firmware_version ?? 'Unknown'}
            </span>

            <span className="inline-flex items-center gap-2 rounded-xl border border-slate-200 bg-white/85 px-4 py-2 text-sm text-slate-600 backdrop-blur">
              <BellRing size={16} />
              {activeAlarms.length} active alarms
            </span>
          </div>
        </div>
      </article>

      <div className="mt-5 grid gap-4 sm:grid-cols-2 xl:grid-cols-5">
        <SummaryCard title="Total Devices" value={summary.total_devices} icon={Boxes} color="cyan" />
        <SummaryCard title="Online Devices" value={summary.online_devices} icon={CircleCheck} color="green" />
        <SummaryCard title="Offline Devices" value={summary.offline_devices} icon={CircleX} color="red" />
        <SummaryCard title="Active Alarms" value={summary.active_alarms} icon={BellRing} color="amber" />
        <SummaryCard title="Critical Alarms" value={summary.critical_alarms} icon={AlertTriangle} color="red" />
      </div>

      <div className="mt-7 flex items-end justify-between gap-4">
        <div>
          <p className="text-xs font-semibold uppercase tracking-[0.18em] text-cyan-600">
            Complete live snapshot
          </p>
          <h2 className="mt-2 text-xl font-bold text-slate-900">
            All Sensor Signals
          </h2>
        </div>
        <span className="rounded-xl bg-slate-100 px-3 py-2 text-xs font-medium text-slate-500">
          {currentSignals.length} signals
        </span>
      </div>

      <div className="mt-4 grid gap-4 sm:grid-cols-2 xl:grid-cols-4">
        {currentSignals.map((signal) => {
          const state = signalState(
            signal,
            activeAlarms,
            primaryOnline,
          )

          const running =
            signal.value === true
            || signal.value === 1
            || String(signal.value).toLowerCase()
              === 'running'

          return (
            <article
              key={`${signal.signal_id}-${signal.name}`}
              className="overflow-hidden rounded-2xl border border-slate-200 bg-white p-4"
            >
              <EquipmentVisual
                name={signal.name}
                state={state}
                running={running}
              />

              <div className="border-t border-slate-100 pt-3">
                <div className="flex items-start justify-between gap-3">
                  <div>
                    <p className="text-sm font-medium text-slate-600">
                      {signalLabel(signal.name)}
                    </p>
                    <p className="mt-1 text-2xl font-bold text-slate-900">
                      {displaySignalValue(signal)}
                    </p>
                  </div>
                  <span
                    className={[
                      'mt-1 size-2.5 rounded-full',
                      state === 'normal'
                        ? 'bg-emerald-400'
                        : state === 'warning'
                          ? 'bg-amber-400'
                          : state === 'alarm'
                            ? 'animate-pulse bg-red-500'
                            : 'bg-slate-400',
                    ].join(' ')}
                  />
                </div>
                <p className="mt-2 text-xs uppercase tracking-wide text-slate-400">
                  Quality: {signal.quality ?? 'unknown'}
                </p>
              </div>
            </article>
          )
        })}

        {currentSignals.length === 0 && (
          <div className="col-span-full rounded-2xl border border-dashed border-slate-700 bg-slate-900/40 p-8 text-center text-slate-500">
            No current signal values are available.
          </div>
        )}
      </div>

      <article className="mt-5 overflow-hidden rounded-2xl border border-slate-200 bg-white">
        <div className="flex items-center justify-between border-b border-slate-200 px-6 py-5">
          <div>
            <h2 className="font-semibold text-slate-900">
              Recent Alarm History
            </h2>
            <p className="mt-1 text-sm text-slate-500">
              Every stored transition, including returned and closed events
            </p>
          </div>
          <span className="rounded-xl bg-slate-100 px-3 py-2 text-xs font-medium text-slate-600">
            {alarmHistory.length} records
          </span>
        </div>

        <div className="divide-y divide-slate-100">
          {alarmHistory.length === 0 && (
            <div className="p-8 text-center text-slate-500">
              No alarm history is available.
            </div>
          )}

          {alarmHistory.slice(0, 12).map((alarm) => (
            <div
              key={`${alarm.device_id}-${alarm.record_id}-${alarm.transition_sequence ?? 0}`}
              className="flex flex-col justify-between gap-3 px-6 py-4 sm:flex-row sm:items-center"
            >
              <div>
                <p className="font-medium text-slate-900">
                  {alarm.alarm_code ?? 'Unknown Alarm'}
                </p>
                <p className="mt-1 text-sm text-slate-500">
                  Record #{alarm.record_id} · {alarm.transition ?? 'Unknown transition'}
                </p>
              </div>
              <span className={`w-fit rounded-full px-2.5 py-1 text-xs font-semibold ${severityClass(alarm.severity)}`}>
                {alarm.severity ?? 'Unknown'}
              </span>
            </div>
          ))}
        </div>
      </article>

      <article className="mt-5 rounded-3xl border border-slate-200 bg-white p-4 sm:p-6">
        <div className="mb-5 flex flex-col justify-between gap-2 sm:flex-row sm:items-end">
          <div>
            <p className="text-xs font-semibold uppercase tracking-[0.18em] text-cyan-600">
              Interactive plant model
            </p>
            <h2 className="mt-2 text-xl font-bold text-slate-900">
              Cooling System Overview
            </h2>
            <p className="mt-1 text-sm text-slate-500">
              Equipment colors and motion respond to live signal quality and active alarms.
            </p>
          </div>
          <span className="rounded-xl bg-slate-100 px-3 py-2 text-xs font-medium text-slate-500">
            Live · Read Only
          </span>
        </div>

        <Suspense
          fallback={(
            <div className="flex h-[560px] items-center justify-center gap-3 rounded-2xl bg-slate-50 text-slate-500">
              <RefreshCw className="animate-spin" size={20} />
              Loading 3D Digital Twin...
            </div>
          )}
        >
          <DigitalTwin3D
            online={primaryOnline}
            signals={currentSignals}
            alarms={activeAlarms}
          />
        </Suspense>
      </article>

      <div className="mt-5 grid gap-5 xl:grid-cols-[1.5fr_1fr]">
        <article className="rounded-2xl border border-slate-800 bg-slate-900/70 p-6">
          <div className="flex items-start justify-between gap-4">
            <div>
              <h2 className="font-semibold text-white">
                Performance Trend
              </h2>
              <p className="mt-1 text-sm text-slate-500">
                {trendSignalName
                  ? `${signalLabel(trendSignalName)} · latest snapshots`
                  : 'Waiting for numeric telemetry'}
              </p>
            </div>
            <Link
              to="/telemetry"
              className="text-sm font-medium text-cyan-400 hover:text-cyan-300"
            >
              View Telemetry
            </Link>
          </div>

          {trendData.length === 0 ? (
            <div className="flex h-72 items-center justify-center text-slate-500">
              No trend data available.
            </div>
          ) : (
            <div className="mt-6 h-72">
              <ResponsiveContainer width="100%" height="100%">
                <LineChart data={trendData}>
                  <CartesianGrid stroke="#1e293b" strokeDasharray="4 4" />
                  <XAxis dataKey="time" stroke="#64748b" minTickGap={28} tick={{ fontSize: 11 }} />
                  <YAxis stroke="#64748b" width={65} tick={{ fontSize: 11 }} unit={trendSignalUnit ? ` ${trendSignalUnit}` : undefined} />
                  <Tooltip contentStyle={{ background: '#0f172a', border: '1px solid #334155', borderRadius: '12px', color: '#e2e8f0' }} />
                  <Line type="monotone" dataKey="value" stroke="#22d3ee" strokeWidth={2.5} dot={false} activeDot={{ r: 5, fill: '#22d3ee' }} />
                </LineChart>
              </ResponsiveContainer>
            </div>
          )}
        </article>

        <article className="overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/70">
          <div className="flex items-center justify-between border-b border-slate-800 px-6 py-5">
            <div>
              <h2 className="font-semibold text-white">
                Active Alarms
              </h2>
              <p className="mt-1 text-sm text-slate-500">
                Current unresolved events
              </p>
            </div>
            <Link
              to="/alarms"
              className="text-sm font-medium text-cyan-400 hover:text-cyan-300"
            >
              View All
            </Link>
          </div>

          <div className="divide-y divide-slate-800">
            {activeAlarms.length === 0 && (
              <div className="p-10 text-center">
                <CircleCheck className="mx-auto text-emerald-400" size={30} />
                <p className="mt-3 text-slate-400">
                  No active alarms.
                </p>
              </div>
            )}

            {activeAlarms.slice(0, 5).map((alarm) => (
              <div
                key={`${alarm.device_id}-${alarm.record_id}`}
                className="px-6 py-5"
              >
                <div className="flex items-start justify-between gap-4">
                  <div>
                    <p className="font-medium text-white">
                      {alarm.alarm_code ?? 'Unknown Alarm'}
                    </p>
                    <p className="mt-1 text-sm text-slate-500">
                      {alarm.device_id} · {alarm.transition ?? 'Active'}
                    </p>
                  </div>
                  <span className={`rounded-full px-2.5 py-1 text-xs font-semibold ${severityClass(alarm.severity)}`}>
                    {alarm.severity ?? 'Unknown'}
                  </span>
                </div>
              </div>
            ))}
          </div>
        </article>
      </div>

      <div className="mt-5 grid gap-4 md:grid-cols-3">
        <div className="flex items-center gap-4 rounded-2xl border border-slate-800 bg-slate-900/70 p-5">
          <div className="flex size-11 items-center justify-center rounded-xl bg-emerald-500/10 text-emerald-400">
            <Wifi size={21} />
          </div>
          <div>
            <p className="text-sm text-slate-500">Cloud Connection</p>
            <p className="mt-1 font-semibold text-white">
              {primaryDevice?.mqtt_connected ? 'MQTT Connected' : 'MQTT Disconnected'}
            </p>
          </div>
        </div>

        <div className="flex items-center gap-4 rounded-2xl border border-slate-800 bg-slate-900/70 p-5">
          <div className="flex size-11 items-center justify-center rounded-xl bg-cyan-500/10 text-cyan-400">
            <Activity size={21} />
          </div>
          <div>
            <p className="text-sm text-slate-500">Telemetry Samples</p>
            <p className="mt-1 font-semibold text-white">
              {telemetry.length} loaded
            </p>
          </div>
        </div>

        <div className="flex items-center gap-4 rounded-2xl border border-slate-800 bg-slate-900/70 p-5">
          <div className="flex size-11 items-center justify-center rounded-xl bg-violet-500/10 text-violet-400">
            <Clock3 size={21} />
          </div>
          <div>
            <p className="text-sm text-slate-500">Dashboard Update</p>
            <p className="mt-1 font-semibold text-white">
              Every 10 seconds
            </p>
          </div>
        </div>
      </div>
    </section>
  )
}


export default DashboardPage
