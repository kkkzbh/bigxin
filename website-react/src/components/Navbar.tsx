import { useState, useEffect } from "react";
import { motion, AnimatePresence } from "framer-motion";

interface NavbarProps {
  onFeaturesClick: () => void;
  onScreenshotsClick: () => void;
  onDownloadClick: () => void;
}

export default function Navbar({
  onFeaturesClick,
  onScreenshotsClick,
  onDownloadClick,
}: NavbarProps) {
  const [scrolled, setScrolled] = useState(false);
  const [menuOpen, setMenuOpen] = useState(false);

  useEffect(() => {
    const onScroll = () => setScrolled(window.scrollY > 40);
    window.addEventListener("scroll", onScroll);
    return () => window.removeEventListener("scroll", onScroll);
  }, []);

  return (
    <motion.header
      initial={{ y: -80 }}
      animate={{ y: 0 }}
      transition={{ duration: 0.6, ease: "easeOut" }}
      className={`fixed top-0 left-0 w-full z-50 transition-all duration-300 ${
        scrolled
          ? "bg-[#0b0d14]/80 backdrop-blur-lg shadow-lg"
          : "bg-transparent"
      }`}
    >
      <nav className="max-w-7xl mx-auto px-6 py-4 flex items-center justify-between">
        {/* Brand */}
        <a href="#" className="flex items-center gap-3 group">
          <img
            src="/app_icon.ico"
            alt="聚信图标"
            className="w-10 h-10 rounded-xl shadow-md group-hover:scale-105 transition-transform"
          />
          <div>
            <div className="font-bold text-lg tracking-tight">聚信 JuXin</div>
            <div className="text-xs text-gray-400">轻盈 · 安全 · 即时</div>
          </div>
        </a>

        {/* Desktop Links */}
        <div className="hidden md:flex items-center gap-8 text-sm font-medium text-gray-300">
          <button
            onClick={onFeaturesClick}
            className="hover:text-white transition"
          >
            功能
          </button>
          <button
            onClick={onScreenshotsClick}
            className="hover:text-white transition"
          >
            界面
          </button>
          <button
            onClick={onDownloadClick}
            className="hover:text-white transition"
          >
            下载
          </button>
          <motion.button
            whileHover={{ scale: 1.05 }}
            whileTap={{ scale: 0.95 }}
            onClick={onDownloadClick}
            className="ml-4 px-5 py-2 rounded-full bg-gradient-to-r from-indigo-600 to-purple-600 text-white font-semibold shadow-lg hover:shadow-indigo-500/40 transition"
          >
            立即下载
          </motion.button>
        </div>

        {/* Mobile Hamburger */}
        <button
          className="md:hidden flex flex-col gap-1.5"
          onClick={() => setMenuOpen(!menuOpen)}
          aria-label="Toggle menu"
        >
          <span
            className={`block w-6 h-0.5 bg-white transition-transform ${
              menuOpen ? "rotate-45 translate-y-2" : ""
            }`}
          />
          <span
            className={`block w-6 h-0.5 bg-white transition-opacity ${
              menuOpen ? "opacity-0" : ""
            }`}
          />
          <span
            className={`block w-6 h-0.5 bg-white transition-transform ${
              menuOpen ? "-rotate-45 -translate-y-2" : ""
            }`}
          />
        </button>
      </nav>

      {/* Mobile Menu */}
      <AnimatePresence>
        {menuOpen && (
          <motion.div
            initial={{ height: 0, opacity: 0 }}
            animate={{ height: "auto", opacity: 1 }}
            exit={{ height: 0, opacity: 0 }}
            className="md:hidden bg-[#0b0d14]/95 backdrop-blur-lg overflow-hidden"
          >
            <div className="flex flex-col items-center gap-6 py-6 text-lg font-medium">
              <button
                onClick={() => {
                  onFeaturesClick();
                  setMenuOpen(false);
                }}
              >
                功能
              </button>
              <button
                onClick={() => {
                  onScreenshotsClick();
                  setMenuOpen(false);
                }}
              >
                界面
              </button>
              <button
                onClick={() => {
                  onDownloadClick();
                  setMenuOpen(false);
                }}
              >
                下载
              </button>
              <motion.button
                whileTap={{ scale: 0.95 }}
                onClick={() => {
                  onDownloadClick();
                  setMenuOpen(false);
                }}
                className="px-6 py-3 rounded-full bg-gradient-to-r from-indigo-600 to-purple-600 font-semibold shadow-lg"
              >
                立即下载
              </motion.button>
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </motion.header>
  );
}
