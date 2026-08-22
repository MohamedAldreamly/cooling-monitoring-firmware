import { useQuery } from '@tanstack/react-query'
import {
  Activity,
  RefreshCw,
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

  const telemetryQuery = useQuery({
    queryKey: [
      'telemetry-chart',
      activeDeviceId,
    ],
    queryFn: () =>
      getDeviceTelemetry(
        activeDeviceId,
        100,
      ),
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
          value:
            typeof signal?.value
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
          point.value !== null,
      )
      .reverse()

  const latestPoint =
    selectedSignalValues[
      selectedSignalValues.length - 1
    ]

  const signalUnit =
    latestPoint?.unit ?? ''

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
            Live and historical signal measurements.
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

      <div className="mt-8 grid gap-4 rounded-2xl border border-slate-800 bg-slate-900/60 p-5 md:grid-cols-2">
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
            <div className="mt-8 grid gap-4 md:grid-cols-3">
              <div className="rounded-2xl border border-cyan-500/20 bg-cyan-500/5 p-5">
                <Activity className="text-cyan-400" />

                <p className="mt-4 text-sm text-slate-400">
                  Latest Value
                </p>

                <p className="mt-2 text-3xl font-bold text-white">
                  {latestPoint
                    ? `${latestPoint.value} ${signalUnit}`
                    : 'Unavailable'}
                </p>
              </div>

              <div className="rounded-2xl border border-slate-800 bg-slate-900/60 p-5">
                <p className="text-sm text-slate-400">
                  Quality
                </p>

                <p
                  className={[
                    'mt-3 text-xl font-semibold uppercase',
                    latestPoint?.quality
                      .toLowerCase()
                      === 'good'
                      ? 'text-emerald-400'
                      : 'text-amber-400',
                  ].join(' ')}
                >
                  {latestPoint?.quality
                    ?? 'Unknown'}
                </p>
              </div>

              <div className="rounded-2xl border border-slate-800 bg-slate-900/60 p-5">
                <p className="text-sm text-slate-400">
                  Loaded Samples
                </p>

                <p className="mt-3 text-3xl font-bold text-white">
                  {selectedSignalValues.length}
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
                  Latest 100 Telemetry snapshots
                </p>
              </div>

              {selectedSignalValues.length === 0 ? (
                <div className="flex h-96 items-center justify-center text-slate-500">
                  No numeric values available for this signal.
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
                          signalUnit
                            ? ` ${signalUnit}`
                            : undefined
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
                      />

                      <Line
                        type="monotone"
                        dataKey="value"
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