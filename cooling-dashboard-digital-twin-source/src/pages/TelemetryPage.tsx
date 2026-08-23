import { useQuery } from '@tanstack/react-query'
import {
  Activity,
  Clock3,
  RefreshCw,
  TrendingDown,
  TrendingUp,
} from 'lucide-react'
import { useState } from 'react'
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
  getDevices,
  getDeviceTelemetry,
} from '../api/dashboardApi'


const timeRanges = [
  { label: '15 min', value: '15m', durationMs: 15 * 60 * 1000 },
  { label: '1 hour', value: '1h', durationMs: 60 * 60 * 1000 },
  { label: '24 hours', value: '24h', durationMs: 24 * 60 * 60 * 1000 },
  { label: '7 days', value: '7d', durationMs: 7 * 24 * 60 * 60 * 1000 },
] as const

type TimeRangeValue =
  (typeof timeRanges)[number]['value']


function formatTimestamp(
  timestamp?: number,
) {
  if (!timestamp) {
    return 'Unknown'
  }

  return new Date(
    timestamp,
  ).toLocaleTimeString([], {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  })
}


function TelemetryPage() {
  const [
    selectedDevice,
    setSelectedDevice,
  ] = useState('')

  const [
    selectedSignal,
    setSelectedSignal,
  ] = useState('')

  const [
    selectedRange,
    setSelectedRange,
  ] = useState<TimeRangeValue>('1h')

  const devicesQuery = useQuery({
    queryKey: ['devices'],
    queryFn: getDevices,
    refetchInterval: 30000,
  })

  const devices =
    devicesQuery.data?.devices ?? []

  const activeDeviceId =
    selectedDevice
    || devices[0]?.device_id
    || ''

  const activeRange =
    timeRanges.find(
      (range) =>
        range.value === selectedRange,
    ) ?? timeRanges[1]

  const telemetryQuery = useQuery({
    queryKey: [
      'telemetry-chart',
      activeDeviceId,
      selectedRange,
    ],
    queryFn: () => {
      const to = Date.now()
      const from =
        to - activeRange.durationMs

      return getDeviceTelemetry(
        activeDeviceId,
        500,
        from,
        to,
      )
    },
    enabled: Boolean(activeDeviceId),
    refetchInterval: 10000,
  })

  const telemetry =
    telemetryQuery.data?.telemetry ?? []

  const signalNames = Array.from(
    new Set(
      telemetry.flatMap(
        (record) =>
          record.signals
            ?.map(
              (signal) =>
                signal.name,
            )
            .filter(
              (
                name,
              ): name is string =>
                Boolean(name),
            )
          ?? [],
      ),
    ),
  ).sort()

  const activeSignalName =
    selectedSignal
    || signalNames[0]
    || ''

  const selectedSignalValues =
    telemetry
      .map((record) => {
        const signal =
          record.signals?.find(
            (item) =>
              item.name
              === activeSignalName,
          )

        return {
          timestamp:
            record.received_at_ms,
          time: formatTimestamp(
            record.received_at_ms,
          ),
          rawValue:
            signal?.value
            ?? null,
          chartValue:
            typeof signal?.value
              === 'boolean'
              ? signal.value ? 1 : 0
              : typeof signal?.value
                  === 'number'
                ? signal.value
                : null,
          quality:
            signal?.quality
            ?? 'unknown',
          unit:
            signal?.unit
            ?? '',
        }
      })
      .filter(
        (point) =>
          point.rawValue !== null,
      )
      .reverse()

  const latestPoint =
    selectedSignalValues[
      selectedSignalValues.length - 1
    ]

  const signalUnit =
    latestPoint?.unit ?? ''

  const isBooleanSignal =
    selectedSignalValues.some(
      (point) =>
        typeof point.rawValue
        === 'boolean',
    )

  const numericValues =
    selectedSignalValues
      .filter(
        (point) =>
          typeof point.rawValue
          === 'number',
      )
      .map(
        (point) => point.chartValue,
      )
      .filter(
        (value): value is number =>
          value !== null,
      )

  const booleanValues =
    selectedSignalValues
      .filter(
        (point) =>
          typeof point.rawValue
          === 'boolean',
      )
      .map(
        (point) =>
          point.rawValue as boolean,
      )

  const activeCount =
    booleanValues.filter(Boolean).length

  const inactiveCount =
    booleanValues.length - activeCount

  const activePercentage =
    booleanValues.length > 0
      ? (activeCount / booleanValues.length) * 100
      : null

  const minimumValue =
    numericValues.length > 0
      ? Math.min(...numericValues)
      : null

  const maximumValue =
    numericValues.length > 0
      ? Math.max(...numericValues)
      : null

  const averageValue =
    numericValues.length > 0
      ? numericValues.reduce(
          (total, value) =>
            total + value,
          0,
        ) / numericValues.length
      : null

  function displayValue(
    value: number | null,
  ) {
    if (value === null) {
      return 'Unavailable'
    }

    return Number.isInteger(value)
      ? String(value)
      : value.toFixed(2)
  }

  function displayRawValue(
    value: number | boolean | string | null,
  ) {
    if (value === null) {
      return 'Unavailable'
    }

    if (typeof value === 'boolean') {
      return value ? 'ON' : 'OFF'
    }

    if (typeof value === 'number') {
      return Number.isInteger(value)
        ? String(value)
        : value.toFixed(2)
    }

    return value
  }

  function refreshTelemetry() {
    void Promise.all([
      devicesQuery.refetch(),
      telemetryQuery.refetch(),
    ])
  }

  return (
    <section>
      <div className="flex flex-col justify-between gap-4 md:flex-row md:items-center">
        <div>
          <h1 className="text-3xl font-bold text-white">
            Live Telemetry
          </h1>

          <p className="mt-2 text-slate-400">
            Read-only live and historical signal measurements.
          </p>
        </div>

        <button
          type="button"
          onClick={refreshTelemetry}
          disabled={
            devicesQuery.isFetching
            || telemetryQuery.isFetching
          }
          className="flex items-center justify-center gap-2 rounded-xl border border-slate-700 bg-slate-900 px-4 py-2.5 text-sm text-slate-300 disabled:opacity-50"
        >
          <RefreshCw
            size={17}
            className={
              telemetryQuery.isFetching
                ? 'animate-spin'
                : ''
            }
          />

          Refresh
        </button>
      </div>

      <div className="mt-8 grid gap-4 rounded-2xl border border-slate-800 bg-slate-900/60 p-5 lg:grid-cols-[1fr_1fr_auto]">
        <label>
          <span className="mb-2 block text-sm text-slate-400">
            Device
          </span>

          <select
            value={activeDeviceId}
            onChange={(event) => {
              setSelectedDevice(
                event.target.value,
              )

              setSelectedSignal('')
            }}
            className="w-full rounded-xl border border-slate-700 bg-slate-950 px-4 py-3 text-white outline-none focus:border-cyan-500"
          >
            {devices.map((device) => (
              <option
                key={device.device_id}
                value={device.device_id}
              >
                {device.device_id}
              </option>
            ))}
          </select>
        </label>

        <label>
          <span className="mb-2 block text-sm text-slate-400">
            Signal
          </span>

          <select
            value={activeSignalName}
            onChange={(event) =>
              setSelectedSignal(
                event.target.value,
              )
            }
            disabled={
              signalNames.length === 0
            }
            className="w-full rounded-xl border border-slate-700 bg-slate-950 px-4 py-3 text-white outline-none focus:border-cyan-500 disabled:opacity-50"
          >
            {signalNames.map((name) => (
              <option
                key={name}
                value={name}
              >
                {name}
              </option>
            ))}
          </select>
        </label>

        <div>
          <span className="mb-2 block text-sm text-slate-400">
            Time range
          </span>

          <div className="flex flex-wrap gap-2">
            {timeRanges.map((range) => (
              <button
                key={range.value}
                type="button"
                onClick={() =>
                  setSelectedRange(
                    range.value,
                  )
                }
                className={[
                  'rounded-lg border px-3 py-3 text-sm transition',
                  selectedRange
                    === range.value
                    ? 'border-cyan-400 bg-cyan-400/10 text-cyan-300'
                    : 'border-slate-700 bg-slate-950 text-slate-400 hover:text-white',
                ].join(' ')}
              >
                {range.label}
              </button>
            ))}
          </div>
        </div>
      </div>

      {telemetryQuery.isLoading && (
        <div className="flex min-h-80 items-center justify-center">
          <RefreshCw
            className="animate-spin text-cyan-400"
            size={32}
          />
        </div>
      )}

      {telemetryQuery.isError && (
        <div className="mt-8 rounded-2xl border border-red-500/20 bg-red-500/5 p-8 text-red-400">
          Unable to load Telemetry data.
        </div>
      )}

      {!telemetryQuery.isLoading
        && !telemetryQuery.isError
        && (
          <>
            <div className="mt-8 grid gap-4 sm:grid-cols-2 xl:grid-cols-4">
              <div className="rounded-2xl border border-cyan-500/20 bg-cyan-500/5 p-5">
                <Activity className="text-cyan-400" />

                <p className="mt-4 text-sm text-slate-400">
                  Latest Value
                </p>

                <p className="mt-2 text-3xl font-bold text-white">
                  {latestPoint
                    ? `${displayRawValue(latestPoint.rawValue)} ${signalUnit}`
                    : 'Unavailable'}
                </p>

                <p className="mt-2 text-xs text-slate-500">
                  Quality: {latestPoint?.quality
                    ?? 'unknown'}
                </p>
              </div>

              <div className="rounded-2xl border border-slate-800 bg-slate-900/60 p-5">
                <TrendingDown className="text-blue-400" />

                <p className="mt-4 text-sm text-slate-400">
                  {isBooleanSignal
                    ? 'Inactive Samples'
                    : 'Minimum'}
                </p>

                <p className="mt-2 text-3xl font-bold text-white">
                  {isBooleanSignal
                    ? inactiveCount
                    : displayValue(minimumValue)}{' '}
                  {!isBooleanSignal
                    && minimumValue !== null
                    ? signalUnit
                    : ''}
                </p>
              </div>

              <div className="rounded-2xl border border-slate-800 bg-slate-900/60 p-5">
                <TrendingUp className="text-amber-400" />

                <p className="mt-4 text-sm text-slate-400">
                  {isBooleanSignal
                    ? 'Active Samples'
                    : 'Maximum'}
                </p>

                <p className="mt-2 text-3xl font-bold text-white">
                  {isBooleanSignal
                    ? activeCount
                    : displayValue(maximumValue)}{' '}
                  {!isBooleanSignal
                    && maximumValue !== null
                    ? signalUnit
                    : ''}
                </p>
              </div>

              <div className="rounded-2xl border border-slate-800 bg-slate-900/60 p-5">
                <Clock3 className="text-violet-400" />

                <p className="mt-4 text-sm text-slate-400">
                  {isBooleanSignal
                    ? 'Active Percentage'
                    : 'Average'}
                </p>

                <p className="mt-2 text-3xl font-bold text-white">
                  {isBooleanSignal
                    ? activePercentage === null
                      ? 'Unavailable'
                      : `${activePercentage.toFixed(1)}%`
                    : displayValue(averageValue)}{' '}
                  {!isBooleanSignal
                    && averageValue !== null
                    ? signalUnit
                    : ''}
                </p>

                <p className="mt-2 text-xs text-slate-500">
                  {selectedSignalValues.length}
                  {isBooleanSignal
                    ? ' state samples'
                    : ' numeric samples'}
                </p>
              </div>
            </div>

            <article className="mt-8 rounded-2xl border border-slate-800 bg-slate-900/60 p-6">
              <div>
                <h2 className="font-semibold text-white">
                  {activeSignalName
                    || 'Signal History'}
                </h2>

                <p className="mt-1 text-sm text-slate-500">
                  {activeRange.label} history · latest 500 snapshots maximum
                </p>
              </div>

              {selectedSignalValues.length === 0 ? (
                <div className="flex h-96 items-center justify-center text-slate-500">
                  No values available for this signal.
                </div>
              ) : (
                <div className="mt-8 h-96 w-full">
                  <ResponsiveContainer
                    width="100%"
                    height="100%"
                  >
                    <LineChart
                      data={
                        selectedSignalValues
                      }
                    >
                      <CartesianGrid
                        stroke="#1e293b"
                        strokeDasharray="4 4"
                      />

                      <XAxis
                        dataKey="time"
                        stroke="#64748b"
                        tick={{
                          fontSize: 12,
                        }}
                      />

                      <YAxis
                        stroke="#64748b"
                        tick={{
                          fontSize: 12,
                        }}
                        unit={
                          !isBooleanSignal
                          && signalUnit
                            ? ` ${signalUnit}`
                            : undefined
                        }
                        domain={
                          isBooleanSignal
                            ? [0, 1]
                            : undefined
                        }
                        ticks={
                          isBooleanSignal
                            ? [0, 1]
                            : undefined
                        }
                        tickFormatter={(value) =>
                          isBooleanSignal
                            ? Number(value) === 1
                              ? 'ON'
                              : 'OFF'
                            : String(value)
                        }
                      />

                      <Tooltip
                        contentStyle={{
                          background:
                            '#0f172a',
                          border:
                            '1px solid #334155',
                          borderRadius:
                            '12px',
                          color: '#e2e8f0',
                        }}
                        formatter={(value) => [
                          isBooleanSignal
                            ? Number(value) === 1
                              ? 'ON'
                              : 'OFF'
                            : `${String(value)} ${signalUnit}`,
                          activeSignalName,
                        ]}
                      />

                      <Line
                        type={
                          isBooleanSignal
                            ? 'stepAfter'
                            : 'monotone'
                        }
                        dataKey="chartValue"
                        stroke="#22d3ee"
                        strokeWidth={2}
                        dot={false}
                        activeDot={{
                          r: 5,
                          fill: '#22d3ee',
                        }}
                        connectNulls
                      />
                    </LineChart>
                  </ResponsiveContainer>
                </div>
              )}
            </article>
          </>
        )}
    </section>
  )
}


export default TelemetryPage
