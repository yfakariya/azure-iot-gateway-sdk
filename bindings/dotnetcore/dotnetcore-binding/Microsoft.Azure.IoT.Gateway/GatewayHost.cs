using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Text;
using System.Threading;

namespace Microsoft.Azure.IoT.Gateway
{
    public static class GatewayHost
    {
        private static readonly Dictionary<IntPtr, GatewayModule> _knownModules = new Dictionary<IntPtr, GatewayModule>();
        private static readonly ReaderWriterLockSlim _knownModulesLock = new ReaderWriterLockSlim( LockRecursionPolicy.NoRecursion );

        public static int Create( IntPtr moduleHandle, IntPtr configHandle )
        {
            try
            {
                CreateCore( moduleHandle, configHandle );
                return 0; // S_OK
            }
            catch (Exception ex)
            {
                return ex.HResult;
            }
        }

        public static int Destroy( IntPtr moduleHandle )
        {
            try
            {
                DestroyCore(moduleHandle);
                return 0; // S_OK
            }
            catch (Exception ex)
            {
                return ex.HResult;
            }
        }

        public static int Receive( IntPtr moduleHandle, IntPtr messageByte, int length )
        {
            try
            {
                ReceiveCore( moduleHandle, messageByte, length );
                return 0; // S_OK
            }
            catch (Exception ex)
            {
                return ex.HResult;
            }
        }

        private static void CreateCore( IntPtr moduleHandle, IntPtr configHandle )
        {
            var header = Marshal.PtrToStructure<DotNetCoreHostHandleDataHeader>( moduleHandle );
            var config = Marshal.PtrToStructure<DotNetCoreHostConfig>( configHandle );

            var module = CreateGatewayModule( config, moduleHandle, header.MessageBusHandle );
            _knownModulesLock.EnterWriteLock();
            try
            {
                _knownModules.Add( moduleHandle, module);
            }
            finally
            {
                _knownModulesLock.ExitWriteLock();
            }
        }

        private static GatewayModule CreateGatewayModule( DotNetCoreHostConfig config, IntPtr moduleHandle, IntPtr messageBusHandle )
        {
            var assembly = Assembly.Load( AssemblyLoadContext.GetAssemblyName( GetUtf8String( config.ModulePathUtf8 ) ) );
            var type = assembly.GetType( GetUtf8String( config.ModuleEntryClassUtf8 ) );
            var configuration = GetUtf8String( config.ModuleArgsJsonUtf8 );
            var module = ( GatewayModule ) Activator.CreateInstance( type );
            module.Create( new MessageBus( moduleHandle, messageBusHandle ), configuration );
            return module;
        }

        private static unsafe string GetUtf8String( IntPtr nullTerminatedUtf8 )
        {
            int length = 0;
            for ( byte* c = ( byte* ) nullTerminatedUtf8.ToPointer(); (*c) != 0; c++ )
            {
                length++;
            }

            return Encoding.UTF8.GetString( ( byte* ) nullTerminatedUtf8.ToPointer(), length );
        }

        private static void DestroyCore( IntPtr moduleHandle )
        {
            // This is OK because this is called in own therad -- other modules are not affected.
            GatewayModule module;
            _knownModulesLock.EnterWriteLock();
            try
            {
                module = _knownModules[ moduleHandle ];
                _knownModules.Remove( moduleHandle );
            }
            finally
            {
                _knownModulesLock.ExitWriteLock();
            }

            module.Dispose();
        }

        private static unsafe void ReceiveCore( IntPtr moduleHandle, IntPtr messageByte, int length )
        {

            // This is OK because this is called in own therad -- other modules are not affected.
            GatewayModule module;
            _knownModulesLock.EnterReadLock();
            try
            {
                module = _knownModules[moduleHandle];
            }
            finally
            {
                _knownModulesLock.ExitReadLock();
            }

            byte[] bytes = new byte[length];
            fixed ( byte* pbyte = bytes )
            {
                Buffer.MemoryCopy( messageByte.ToPointer(), pbyte, length, length );
            }

            module.Receive( new Message( bytes ) );
        }
    }

    internal struct DotNetCoreHostHandleDataHeader
    {
        public IntPtr MessageBusHandle;
    }

    internal struct DotNetCoreHostConfig
    {
        public IntPtr ModulePathUtf8;
        public IntPtr ModuleEntryClassUtf8;
        public IntPtr ModuleArgsJsonUtf8;
        public IntPtr CoreClrPathUtf8; // not used
    }

    internal sealed class ModuleEntry
    {
        public GatewayModule ManagedModule { get; }
        public IntPtr HostModuleHandle { get; }

        public ModuleEntry( IntPtr hostModuleHandle, GatewayModule managedModule )
        {
            this.HostModuleHandle = hostModuleHandle;
            this.ManagedModule = managedModule;
        }
    }
}
