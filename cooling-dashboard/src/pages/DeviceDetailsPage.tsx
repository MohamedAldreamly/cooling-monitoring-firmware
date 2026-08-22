import { useQuery } from '@tanstack/react-query'
import {
  ArrowLeft,
  BellRing,
  Cloud,
  Database,
  RefreshCw,
  Wifi,
} from 'lucide-react'
import { Link, useParams } from 'react-router-dom'

import {
  getDevice,
  getDeviceAlarms,
  getDeviceTelemetry,
} from '../api/dashboardApi'


function formatSignalValue(
  value: unknown,
  unit?: string,
) {
  if (
    value === null
    || value === undefined
  ) {
    return 'Unavailable'
  }

  if (typeof value === 'boolean') {
    return value ? 'ON' : 'OFF'
  }

  if (
    typeof value === 'number'
    && !Number.isInteger(value)
  ) {
    return `${value.toFixed(2)}${unit ? ` ${unit}` : ''}`
  }

  return `${String(value)}${unit ? ` ${unit}` : ''}`
}


function stateColor(
  connected?: boolean,
) {
  return connected
    ? 'text-emerald-400'
    : 'text-red-400'
}


function DeviceDetailsPage() {
  const { deviceId } = useParams()

  const deviceQuery = useQuery({
    queryKey: ['device', deviceId],
    queryFn: () => getDevice(deviceId!),
    enabled: Boolean(deviceId),
    refetchInterval: 15000,
  })

  const telemetryQuery = useQuery({
    queryKey: [
      'device-telemetry',
      deviceId,
    ],
    queryFn: () =>
      getDeviceTelemetry(
        deviceId!,
        20,
      ),
    enabled: Boolean(deviceId),
    refetchInterval: 10000,
  })

  const alarmsQuery = useQuery({
    queryKey: [
      'device-alarms',
      deviceId,
    ],
    queryFn: () =>
      getDeviceAlarms(
        deviceId!,
        20,
      ),
    enabled: Boolean(deviceId),
    refetchInterval: 15000,
  })

  const isLoading =
    deviceQuery.isLoading
    || telemetryQuery.isLoading
    || alarmsQuery.isLoading

  const hasError =
    deviceQuery.isError
    || telemetryQuery.isError
    || alarmsQuery.isError

  const isRefreshing =
    deviceQuery.isFetching
    || telemetryQuery.isFetching
    || alarmsQuery.isFetching

  const device =
    deviceQuery.data?.device

  const telemetry =
    telemetryQuery.data?.telemetry ?? []

  const alarms =
    alarmsQuery.data?.alarms ?? []

  const latestTelemetry =
    telemetry[0]

  const signals =
    latestTelemetry?.signals ?? []

  function refreshAll() {
    void Promise.all([
      deviceQuery.refetch(),
      telemetryQuery.refetch(),
      alarmsQuery.refetch(),
    ])
  }

  if (!deviceId) {
    return (
      <p className="text-red-400">
        Device ID is missing.
      </p>
    )
  }

  if (isLoading) {
    return (
      <div className="flex min-h-80 items-center justify-center">
        <RefreshCw
          className="animate-spin text-cyan-400"
          size={32}
        />
      </div>
    )
  }

  if (hasError || !device) {
    return (
      <div className="rounded-2xl border border-red-500/20 bg-red-500/5 p-8">
        <h2 className="text-xl font-semibold text-red-400">
          Unable to load device
        </h2>

        <p className="mt-2 text-slate-400">
          Device: {deviceId}
        </p>

        <button
          type="button"
          onClick={refreshAll}
          className="mt-5 rounded-xl bg-red-500 px-5 py-2.5 text-white"
        >
          Try again
        </button>
      </div>
    )
  }

  const online =
    device.status?.toLowerCase()
    === 'online'

  return (
    <section>
      <Link
        to="/devices"
        className="inline-flex items-center gap-2 text-sm text-slate-400 hover:text-cyan-300"
      >
        <ArrowLeft size={17} />
        Back to devices
      </Link>

      <div className="mt-5 flex flex-col justify-between gap-4 md:flex-row md:items-center">
        <div>
          <div className="flex items-center gap-3">
            <h1 className="text-3xl font-bold text-white">
              {device.device_id}
            </h1>

            <span
              className={[
                'rounded-full px-3 py-1 text-xs font-medium',
                online
                  ? 'bg-emerald-500/15 text-emerald-400'
                  : 'bg-red-500/15 text-red-400',
              ].join(' ')}
            >
              {online ? 'Online' : 'Offline'}
            </span>
          </div>

          <p className="mt-2 text-slate-400">
            Firmware {device.firmware_version ?? 'Unknown'}
          </p>
        </div>

        <button
          type="button"
          onClick={refreshAll}
          disabled={isRefreshing}
          className="flex items-center justify-center gap-2 rounded-xl border border-slate-700 bg-slate-900 px-4 py-2.5 text-sm text-slate-300 disabled:opacity-50"
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

      <div className="mt-8 grid gap-4 md:grid-cols-2 xl:grid-cols-4">
        <div className="rounded-2xl border border-slate-800 bg-slate-900/60 p-5">
          <Wifi
            className={stateColor(
              device.wifi_connected,
            )}
          />

          <p className="mt-4 text-sm text-slate-500">
            Wi-Fi
          </p>

          <p className="mt-1 font-semibold text-white">
            {device.wifi_connected
              ? 'Connected'
              : 'Disconnected'}
          </p>
        </div>

        <div className="rounded-2xl border border-slate-800 bg-slate-900/60 p-5">
          <Cloud
            className={stateColor(
              device.mqtt_ready,
            )}
          />

          <p className="mt-4 text-sm text-slate-500">
            MQTT
          </p>

          <p className="mt-1 font-semibold text-white">
            {device.mqtt_ready
              ? 'Ready'
              : 'Not ready'}
          </p>
        </div>

        <div className="rounded-2xl border border-slate-800 bg-slate-900/60 p-5">
          <Database
            className={stateColor(
              device.storage_mounted,
            )}
          />

          <p className="mt-4 text-sm text-slate-500">
            Storage
          </p>

          <p className="mt-1 font-semibold text-white">
            {device.storage_mounted
              ? 'Mounted'
              : 'Unavailable'}
          </p>
        </div>

        <div className="rounded-2xl border border-slate-800 bg-slate-900/60 p-5">
          <BellRing className="text-amber-400" />

          <p className="mt-4 text-sm text-slate-500">
            Alarm Records
          </p>

          <p className="mt-1 font-semibold text-white">
            {alarms.length}
          </p>
        </div>
      </div>

      <article className="mt-8 rounded-2xl border border-slate-800 bg-slate-900/60">
        <div className="border-b border-slate-800 px-6 py-5">
          <h2 className="font-semibold text-white">
            Latest Telemetry
          </h2>

          <p className="mt-1 text-sm text-slate-500">
            Snapshot sequence:{' '}
            {latestTelemetry?.snapshot_sequence ?? 'Unavailable'}
          </p>
        </div>

        {signals.length === 0 ? (
          <p className="p-8 text-center text-slate-500">
            No Telemetry signals available.
          </p>
        ) : (
          <div className="grid gap-px bg-slate-800 sm:grid-cols-2 lg:grid-cols-4">
            {signals.map((signal, index) => (
              <div
                key={
                  signal.signal_id
                  ?? `${signal.name}-${index}`
                }
                className="bg-slate-900 p-5"
              >
                <div className="flex items-center justify-between gap-3">
                  <p className="truncate text-sm text-slate-400">
                    {signal.name ?? `Signal ${index + 1}`}
                  </p>

                  <span
                    className={[
                      'size-2 rounded-full',
                      signal.quality?.toLowerCase()
                        === 'good'
                        ? 'bg-emerald-400'
                        : 'bg-amber-400',
                    ].join(' ')}
                  />
                </div>

                <p className="mt-3 text-xl font-semibold text-white">
                  {formatSignalValue(
                    signal.value,
                    signal.unit,
                  )}
                </p>

                <p className="mt-2 text-xs uppercase text-slate-600">
                  {signal.quality ?? 'Unknown quality'}
                </p>
              </div>
            ))}
          </div>
        )}
      </article>

      <article className="mt-8 overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/60">
        <div className="border-b border-slate-800 px-6 py-5">
          <h2 className="font-semibold text-white">
            Recent Alarm History
          </h2>
        </div>

        <div className="overflow-x-auto">
          <table className="w-full text-left text-sm">
            <thead className="bg-slate-950/70 text-slate-500">
              <tr>
                <th className="px-6 py-4">Record</th>
                <th className="px-6 py-4">Alarm</th>
                <th className="px-6 py-4">Severity</th>
                <th className="px-6 py-4">Transition</th>
                <th className="px-6 py-4">Signal</th>
              </tr>
            </thead>

            <tbody className="divide-y divide-slate-800">
              {alarms.map((alarm) => (
                <tr key={alarm.record_id}>
                  <td className="px-6 py-4 text-slate-400">
                    #{alarm.record_id}
                  </td>

                  <td className="px-6 py-4 font-medium text-white">
                    {alarm.alarm_code ?? 'Unknown'}
                  </td>

                  <td className="px-6 py-4 text-amber-400">
                    {alarm.severity ?? 'Unknown'}
                  </td>

                  <td className="px-6 py-4 text-slate-300">
                    {alarm.transition ?? 'Unknown'}
                  </td>

                  <td className="px-6 py-4 text-slate-400">
                    {alarm.source_signal ?? 'Unknown'}
                  </td>
                </tr>
              ))}

              {alarms.length === 0 && (
                <tr>
                  <td
                    colSpan={5}
                    className="px-6 py-8 text-center text-slate-500"
                  >
                    No alarm records found.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </article>
    </section>
  )
}


export default DeviceDetailsPage